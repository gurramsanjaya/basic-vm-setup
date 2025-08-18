
#ifndef FHASH_MAP_H
#include "hash_map.h"
#endif /** FHASH_MAP_H */

#include <boost/predef/architecture.h>
#include <iterator>

#define SPIN_LIMIT 100

inline void pause_spin()
{
#ifdef BOOST_ARCH_X86
    __builtin_ia32_pause();
#elif defined(BOOST_ARCH_ARM)
    __yield();
#else
    // This is a scheduling method, will be way too slow and wrong
    // on many levels.
    // std::this_thread::yield();
#endif
}

#define ERROR_ON_DESTROYED(track)                                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        if (track.state & DESTROYED_STATE)                                                                     \
        {                                                                                                              \
            throw std::runtime_error("use after destructor call");                                                     \
        }                                                                                                              \
    } while (0)

/**
 * Class FixedSizeLockFreeHashMap
 */
template <typename KeyT, typename ValueT, typename HashFn, typename ProbeFn, typename EqualityKeyFn,
          typename AllocatorValueT>
void FixedSizeLockFreeHashMap<KeyT, ValueT, HashFn, ProbeFn, EqualityKeyFn, AllocatorValueT>::destroy(
    FixedSizeLockFreeHashMap *ptr)
{
    size_t a_size = get_final_aligned_size(ptr->capacity_);
    ptr->~FixedSizeLockFreeHashMap();
    std::allocator<uint8_t>().deallocate((uint8_t *)ptr, a_size);
}

template <typename KeyT, typename ValueT, typename HashFn, typename ProbeFn, typename EqualityKeyFn,
          typename AllocatorValueT>
typename FixedSizeLockFreeHashMap<KeyT, ValueT, HashFn, ProbeFn, EqualityKeyFn, AllocatorValueT>::SomePointer
FixedSizeLockFreeHashMap<KeyT, ValueT, HashFn, ProbeFn, EqualityKeyFn, AllocatorValueT>::create(int capacity)
{
    size_t a_size = get_final_aligned_size(capacity);
    uint8_t *mem = std::allocator<uint8_t>().allocate(a_size);
    try
    {
        new (mem) FixedSizeLockFreeHashMap(capacity);
    }
    catch (...)
    {
        std::allocator<uint8_t>().deallocate(mem, a_size);
        throw;
    }
    SomePointer some_ptr(static_cast<FixedSizeLockFreeHashMap *>((void *)mem));
    Track init{EMPTY_STATE, 0};
    for (int i = 0; i < some_ptr->capacity_; i++)
    {
        some_ptr->map_[i].track.store(init, std::memory_order_relaxed);
    }
    return some_ptr;
}

template <typename KeyT, typename ValueT, typename HashFn, typename ProbeFn, typename EqualityKeyFn,
          typename AllocatorValueT>
typename FixedSizeLockFreeHashMap<KeyT, ValueT, HashFn, ProbeFn, EqualityKeyFn, AllocatorValueT>::OptionalValuePair
FixedSizeLockFreeHashMap<KeyT, ValueT, HashFn, ProbeFn, EqualityKeyFn, AllocatorValueT>::get_value_pair(size_t idx,
                                                                                                        bool weak)
{
    Track ctrack, ntrack;
    if (idx < 0 || idx >= capacity_)
    {
        // may come here... if the iterator = end()/invalid() and its derefernced
        return OptionalValuePair();
    }
    ctrack = map_[idx].track.load(std::memory_order_acquire);
    if (ctrack.state == EMPTY_STATE)
    {
        // Should really not come here
        return OptionalValuePair();
    }
    if (weak)
    {
        // If its the special state, destructor has been called...
        ERROR_ON_DESTROYED(ctrack);
        if (ctrack.state == FILLED_STATE)
        {
            // only when its not locked and filled try getting it
            KeyT ckey = map_[idx].key;
            ValueT cvalue = *(map_[idx].value);
            // weird load-release, but necessary. doesn't reorder the above key, value
            // copy below this
            ntrack = map_[idx].track.load(std::memory_order_release);
            if (ntrack == ctrack)
            {
                // success only when both state and version(ABA issue) is the same
                return OptionalValuePair(ValuePair(ckey, cvalue));
            }
        }
    }
    else
    {
        for (int i = 0; i < SPIN_LIMIT; i++)
        {
            // If its the special state, destructor has been called...
            ERROR_ON_DESTROYED(ctrack);
            if (ctrack.state == FILLED_STATE)
            {
                KeyT ckey = map_[idx].key;
                ValueT cvalue = *(map_[idx].value);
                // load-acq-rel instead of 2 loads per iteratoion
                ntrack = map_[idx].track.load(std::memory_order_acq_rel);
                if (ntrack == ctrack)
                {
                    return OptionalValuePair(ValuePair(ckey, cvalue));
                }
                else
                {
                    // no need for a second load
                    ctrack = ntrack;
                }
            }
            else
            {
                ctrack = map_[idx].track.load(std::memory_order_acquire);
            }
            // falls through here if locked state (empty or filled) or version/state
            // mismatch
            pause_spin();
        }
    }
    return OptionalValuePair();
}

template <typename KeyT, typename ValueT, typename HashFn, typename ProbeFn, typename EqualityKeyFn,
          typename AllocatorValueT>
bool FixedSizeLockFreeHashMap<KeyT, ValueT, HashFn, ProbeFn, EqualityKeyFn, AllocatorValueT>::isNotEmpty(size_t idx)
{
    Track ctrack = map_[idx].track.load(std::memory_order_relaxed);
    // If its the special state, destructor has been called...
    ERROR_ON_DESTROYED(ctrack);
    return !(ctrack.state & EMPTY_STATE);
}

template <typename KeyT, typename ValueT, typename HashFn, typename ProbeFn, typename EqualityKeyFn,
          typename AllocatorValueT>
size_t FixedSizeLockFreeHashMap<KeyT, ValueT, HashFn, ProbeFn, EqualityKeyFn, AllocatorValueT>::find_internal(
    const KeyT &fkey)
{
    Track ctrack;
    size_t idx = HashFn()(fkey) % capacity_;
    for (size_t retries = 0; retries < capacity_; retries++)
    {
        // Needs acquire, to reflect new key that may have been inserted in another
        // thread
        ctrack = map_[idx].track.load(std::memory_order_acquire);
        // If its the special state, destructor has been called...
        ERROR_ON_DESTROYED(ctrack);
        if (!(ctrack.state & EMPTY_STATE) && EqualityKeyFn()(fkey, map_[idx].key))
        {
            return idx;
        }
        else
        {
            idx = ProbeFn()(idx, retries) % capacity_;
        }
    }
    return capacity_;
}

template <typename KeyT, typename ValueT, typename HashFn, typename ProbeFn, typename EqualityKeyFn,
          typename AllocatorValueT>
size_t FixedSizeLockFreeHashMap<KeyT, ValueT, HashFn, ProbeFn, EqualityKeyFn, AllocatorValueT>::insert_internal(
    ValuePair &&value_pair)
{
    Track ctrack, ntrack{LOCKED_STATE, 0};
    size_t idx = HashFn()(std::get<0>(value_pair)) % capacity_;
    for (size_t retries = 0; retries < capacity_; retries++)
    {
        if (size_.load(std::memory_order_relaxed) == capacity_)
        {
            return capacity_;
        }
        ctrack = map_[idx].track.load(std::memory_order_relaxed);
        // don't need to restrict till spin limit here, only spurious fails will
        // make this loop
        do
        {
            // If its the special state, destructor has been called...
            ERROR_ON_DESTROYED(ctrack);
            if (ctrack.state & (EMPTY_STATE | LOCKED_STATE) == (EMPTY_STATE | LOCKED_STATE))
            {
                // Not reachable currenlty, but technically it means someone else has
                // got this
                break;
            }
            pause_spin();
            // Try looping as long as empty state. Needs acquire to keep insertion
            // logic that is below this to not be reordered above this
        } while ((ctrack.state == EMPTY_STATE) &&
                 !map_[idx].track.compare_exchange_weak(ctrack, ntrack, std::memory_order_acquire,
                                                        std::memory_order_relaxed));
        if (ctrack.state == EMPTY_STATE)
        {
            // Actual insertion here, copy assignment
            map_[idx].key = std::get<0>(value_pair);
            map_[idx].value = AllocatorValueT().allocate(1);
            *(map_[idx].value) = std::get<1>(value_pair);
            ntrack = {FILLED_STATE, 1};
            map_[idx].track.store(ntrack, std::memory_order_release);
            size_.fetch_add(1, std::memory_order_relaxed);
            return idx;
        }
        else
        {
            idx = ProbeFn()(idx, retries) % capacity_;
        }
    }
    return capacity_;
}

template <typename KeyT, typename ValueT, typename HashFn, typename ProbeFn, typename EqualityKeyFn,
          typename AllocatorValueT>
size_t FixedSizeLockFreeHashMap<KeyT, ValueT, HashFn, ProbeFn, EqualityKeyFn, AllocatorValueT>::update_internal(
    ValuePair &&value_pair)
{
    Track ctrack, ntrack{FILLED_STATE | LOCKED_STATE, 0};
    size_t idx = HashFn()(std::get<0>(value_pair)) % capacity_;
    for (size_t retries = 0; retries < capacity_; retries++)
    {
        // Need to acquire for key comparison check, might be recently added key
        ctrack = map_[idx].track.load(std::memory_order_acquire);
        // If its the special state, destructor has been called...
        ERROR_ON_DESTROYED(ctrack);
        if (!(ctrack.state & EMPTY_STATE) && EqualityKeyFn()(std::get<0>(value_pair), map_[idx].key))
        {
            for (int i = 0; i < SPIN_LIMIT; i++)
            {
                pause_spin();
                ntrack.version = ctrack.version + 1;
                // If its the special state, destructor has been called...
                ERROR_ON_DESTROYED(ctrack);
                if (ctrack.state & LOCKED_STATE)
                {
                    // wait till its unlocked before exchanging
                    ctrack = map_[i].track.load(std::memory_order_relaxed);
                    continue;
                }
                else if (map_[i].track.compare_exchange_weak(ctrack, ntrack, std::memory_order_acquire,
                                                             std::memory_order_relaxed))
                {
                    // Actual update here
                    *(map_[i].value) = std::get<1>(value_pair);
                    ntrack = {FILLED_STATE, ntrack.version};
                    map_[i].track.store(ntrack, std::memory_order_release);
                    return idx;
                }
            }
            // spurious failure
            return -1;
        }
        else
        {
            idx = ProbeFn()(idx, retries);
        }
    }
    return capacity_;
}

template <typename KeyT, typename ValueT, typename HashFn, typename ProbeFn, typename EqualityKeyFn,
          typename AllocatorValueT>
FixedSizeLockFreeHashMap<KeyT, ValueT, HashFn, ProbeFn, EqualityKeyFn, AllocatorValueT>::FixedSizeLockFreeHashMap(
    size_t capacity)
    : capacity_(capacity), size_(0)
{
}

// We should really not have contention in the destructor...
template <typename KeyT, typename ValueT, typename HashFn, typename ProbeFn, typename EqualityKeyFn,
          typename AllocatorValueT>
FixedSizeLockFreeHashMap<KeyT, ValueT, HashFn, ProbeFn, EqualityKeyFn, AllocatorValueT>::~FixedSizeLockFreeHashMap()
{
    Track ctrack, ftrack{DESTROYED_STATE, 0};
    for (int i = 0; i < capacity_; i++)
    {
        ctrack = map_[i].track.load(std::memory_order_relaxed);
        // Spinnning... state shouldn't be locked here... we are destructing
        while (true)
        {
            pause_spin();
            if (ctrack.state & DESTROYED_STATE)
            {
                // Impossibru!
                break;
            }
            else if (ctrack.state & LOCKED_STATE)
            {
                // By Azura, by Azura, by Azura! It's the Grand Champion!
                ctrack = map_[i].track.load(std::memory_order_relaxed);
                continue;
            }
            else if (map_[i].track.compare_exchange_weak(ctrack, ftrack, std::memory_order_acquire,
                                                         std::memory_order_relaxed))
            {
                // set to destroy only state
                break;
            }
        }
        // deallocate ValueT if the previous state was filled
        if (ctrack.state == FILLED_STATE)
        {
            map_[i].value->~ValueT();
            AllocatorValueT().deallocate(map_[i].value, sizeof(ValueT));
        }
    }
}
