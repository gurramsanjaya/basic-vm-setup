
#include "hash_map.h"

#include <iterator>

#define ERROR_ON_DESTROYED(track)                            \
  do {                                                       \
    if (track.state & Track::kdestroyed_state) {             \
      throw std::runtime_error("use after destructor call"); \
    }                                                        \
  } while (0)


/**
 * Class FixedSizeLockFreeHashMap
 */
template <class KeyT, class ValueT, class HashFn, class ProbeFn,
          class EqualityKeyFn, class AllocatorValueT>
void FixedSizeLockFreeHashMap<
    KeyT, ValueT, HashFn, ProbeFn, EqualityKeyFn,
    AllocatorValueT>::destroy(FixedSizeLockFreeHashMap* ptr) {
  size_t a_size = get_final_aligned_size(ptr->capacity_);
  ptr->~FixedSizeLockFreeHashMap();
  std::allocator<uint8_t>().deallocate(ptr, a_size);
}

template <class KeyT, class ValueT, class HashFn, class ProbeFn,
          class EqualityKeyFn, class AllocatorValueT>
FixedSizeLockFreeHashMap<KeyT, ValueT, HashFn, ProbeFn, EqualityKeyFn,
                         AllocatorValueT>::SomePointer
FixedSizeLockFreeHashMap<KeyT, ValueT, HashFn, ProbeFn, EqualityKeyFn,
                         AllocatorValueT>::create(int capacity =
                                                      kcapacity_max) {
  size_t a_size = get_final_aligned_size(capacity);
  uint8_t* mem = std::allocator<uint8_t>().allocate(a_size);
  try {
    new (mem) FixedSizeLockFreeHashMap(capacity);
  } catch (...) {
    std::allocator<uint8_t>().deallocate(mem, a_size);
    throw;
  }
  SomePointer some_ptr(static_cast<FixedSizeLockFreeHashMap*>((void*)mem));
  Track init{Track::kempty_state, 0};
  for (int i = 0; i < some_ptr->capacity_; i++) {
    some_ptr->map_[i].track.store(init, std::memory_order_relaxed);
  }
  return some_ptr;
}

template <class KeyT, class ValueT, class HashFn, class ProbeFn,
          class EqualityKeyFn, class AllocatorValueT>
FixedSizeLockFreeHashMap<KeyT, ValueT, HashFn, ProbeFn, EqualityKeyFn,
                         AllocatorValueT>::OptionalValuePair
FixedSizeLockFreeHashMap<KeyT, ValueT, HashFn, ProbeFn, EqualityKeyFn,
                         AllocatorValueT>::get_value_pair(size_t idx,
                                                          bool weak = true) {
  Track ctrack, ntrack;
  if (idx < 0 || idx >= capacity_) {
    // may come here... if the iterator = end()/invalid() and its derefernced
    return OptionalValuePair();
  }
  ctrack = map_[idx].track.load(std::memory_order_acquire);
  if (ctrack.state == Track::kempty_state) {
    // Should really not come here
    return OptionalValuePair();
  }
  if (weak) {
    // If its the special state, destructor has been called...
    ERROR_ON_DESTROYED(ctrack));
    if (ctrack.state == Track::kfilled_state) {
      // only when its not locked and filled try getting it
      KeyT ckey = map_[idx].key;
      ValueT cvalue = map_[idx].value*;
      // weird load-release, but necessary. doesn't reorder the above key, value
      // copy below this
      ntrack = map_[idx].track.load(std::memory_order_release);
      if (ntrack == ctrack) {
        // success only when both state and version(ABA issue) is the same
        return OptionalValuePair(ckey, cvalue);
      }
    }
  } else {
    for (int i = 0; i < SPIN_LIMIT; i++) {
      // If its the special state, destructor has been called...
      ERROR_ON_DESTROYED(ctrack);
      if (ctrack.state == Track::kfilled_state) {
        KeyT ckey = map_[idx].key;
        ValueT cvalue = map_[idx].value*;
        // load-acq-rel instead of 2 loads per iteratoion
        ntrack = map_[idx].track.load(std::memory_order_acq_rel);
        if (ntrack == ctrack) {
          return OptionalValuePair(ckey, cvalue);
        } else {
          // no need for a second load
          ctrack = ntrack;
        }
      } else {
        ctrack = map_[idx].track.load(std::memory_order_acquire);
      }
      // falls through here if locked state (empty or filled) or version/state
      // mismatch
      pause_spin();
    }
  }
  return OptionalValuePair();
}

template <class KeyT, class ValueT, class HashFn, class ProbeFn,
          class EqualityKeyFn, class AllocatorValueT>
bool FixedSizeLockFreeHashMap<KeyT, ValueT, HashFn, ProbeFn, EqualityKeyFn,
                              AllocatorValueT>::isNotEmpty(size_t idx) {
  Track ctrack = map_[idx].track.load(std::memory_order_relaxed);
  // If its the special state, destructor has been called...
  ERROR_ON_DESTROYED(ctrack);
  return !(ctrack.state & Track::kempty_state);
}

template <class KeyT, class ValueT, class HashFn, class ProbeFn,
          class EqualityKeyFn, class AllocatorValueT>
size_t
FixedSizeLockFreeHashMap<KeyT, ValueT, HashFn, ProbeFn, EqualityKeyFn,
                         AllocatorValueT>::find_internal(const KeyT& fkey) {
  Track ctrack;
  size_t idx = HashFn()(fkey) % capacity_;
  for (size_t retries = 0; retries < capacity_; retries++) {
    // Needs acquire, to reflect new key that may have been inserted in another
    // thread
    ctrack = map_[idx].track.load(std::memory_order_acquire);
    // If its the special state, destructor has been called...
    ERROR_ON_DESTROYED(ctrack);
    if (!(ctrack.state & Track::kempty_state) &&
        EqualityKeyFn()(fkey, map_[idx].key)) {
      return idx;
    } else {
      idx = ProbeFn()(idx, retries) % capacity_;
    }
  }
  return capacity_;
}

template <class KeyT, class ValueT, class HashFn, class ProbeFn,
          class EqualityKeyFn, class AllocatorValueT>
size_t FixedSizeLockFreeHashMap<
    KeyT, ValueT, HashFn, ProbeFn, EqualityKeyFn,
    AllocatorValueT>::insert_internal(ValuePair&& value_pair) {
  Track ctrack, ntrack{Track::klocked_state, 0};
  size_t idx = HashFn()(std::get<0>(value_pair)) % capacity_;
  for (size_t retries = 0; retries < capacity_; retries++) {
    if (size_.load(std::memory_order_relaxed) == capacity_) {
      return capacity_;
    }
    ctrack = map_[idx].track.load(std::memory_order_relaxed);
    // don't need to restrict till spin limit here, only spurious fails will
    // make this loop
    do {
      // If its the special state, destructor has been called...
      ERROR_ON_DESTROYED(ctrack);
      if (ctrack.state & (Track::kempty_state && Track::klocked_state)) {
        // Not reachable currenlty, but technically it means someone else has
        // got this
        break;
      }
      pause_spin();
      // Try looping as long as empty state. Needs acquire to keep insertion
      // logic that is below this to not be reordered above this
    } while ((ctrack.state == Track::kempty_state) &&
             !map_[i].track.compare_exchange_weak(ctrack, ntrack,
                                                  std::memory_order_acquire,
                                                  std::memory_order_relaxed));
    if (ctrack.state == Track::kempty_state) {
      // Actual insertion here, copy assignment
      map_[idx].key = std::get<0>(value_pair);
      map_[idx].value = AllocatorValueT().allocate(1);
      *(map_[idx].value) = std::get<1>(value_pair);
      ntrack = {Track::kfilled_state, 1};
      map_[idx].track.store(ntrack, std::memory_order_release);
      size_.fetch_add(1, std::memory_order_relaxed);
      return idx;
    } else {
      idx = ProbeFn()(idx, retries) % capacity_;
    }
  }
  return capacity_;
}

template <class KeyT, class ValueT, class HashFn, class ProbeFn,
          class EqualityKeyFn, class AllocatorValueT>
size_t FixedSizeLockFreeHashMap<
    KeyT, ValueT, HashFn, ProbeFn, EqualityKeyFn,
    AllocatorValueT>::update_internal(ValuePair&& value_pair) {
  Track ctrack, ntrack{Track::kfilled_state | Track::klocked_state, 0};
  size_t idx = HashFn()(std::get<0>(value_pair)) % capacity_;
  for (int retries = 0; retries < capacity_; retries++) {
    // Need to acquire for key comparison check, might be recently added key
    ctrack = map_[i].track.load(std::memory_order_acquire);
    // If its the special state, destructor has been called...
    ERROR_ON_DESTROYED(ctrack);
    if (!(ctrack.state & Track::kempty_state) &&
        EqualityKeyFn()(std::get<0>(value_pair), map_[idx].key)) {
      for (int i = 0; i < SPIN_LIMIT; i++) {
        pause_spin();
        ntrack.version = ctrack.version + 1;
        // If its the special state, destructor has been called...
        ERROR_ON_DESTROYED(ctrack);
        if (ctrack.state & Track::klocked_state) {
          // wait till its unlocked before exchanging
          ctrack = map_[i].track.load(std::memory_order_relaxed);
          continue;
        } else if (map_[i].track.compare_exchange_weak(
                       ctrack, ntrack, std::memory_order_acquire,
                       std::memory_order_relaxed)) {
          // Actual update here
          map_[i].value* = std::get<1>(value_pair);
          ntrack = {Track::kfilled_state, ntrack.version};
          map_[i].track.store(ntrack, std::memory_order_release);
          return idx;
        }
      }
      // spurious failure
      return -1;
    } else {
      idx = ProbeFn()(idx, retries);
    }
  }
  return capacity_;
}

template <class KeyT, class ValueT, class HashFn, class ProbeFn,
          class EqualityKeyFn, class AllocatorValueT>
FixedSizeLockFreeHashMap<
    KeyT, ValueT, HashFn, ProbeFn, EqualityKeyFn,
    AllocatorValueT>::FixedSizeLockFreeHashMap(size_t capacity)
    : capacity_(capacity), size_(0) {}

// We should really not have contention in the destructor...
template <class KeyT, class ValueT, class HashFn, class ProbeFn,
          class EqualityKeyFn, class AllocatorValueT>
FixedSizeLockFreeHashMap<KeyT, ValueT, HashFn, ProbeFn, EqualityKeyFn,
                         AllocatorValueT>::~FixedSizeLockFreeHashMap() {
  Track ctrack, ftrack{Track::kdestroyed_state, 0};
  for (int i = 0; i < capacity_; i++) {
    ctrack = map_[i].track.load(std::memory_order_relaxed);
    // Spinnning... state shouldn't be locked here... we are destructing
    while (true) {
      pause_spin();
      if (ctrack.state & Track::kdestroyed_state) {
        // Impossibru!
        break;
      } else if (ctrack.state & Track::klocked_state) {
        // By Azura, by Azura, by Azura! It's the Grand Champion!
        ctrack = map_[i].track.load(std::memory_order_relaxed);
        continue;
      } else if (map_[i].track.compare_exchange_weak(
                     ctrack, ftrack, std::memory_order_acquire,
                     std::memory_order_relaxed)) {
        // set to destroy only state
        break;
      }
    }
    // deallocate ValueT if the previous state was filled
    if (ctrack.state == Track::kfilled_state) {
      map_[i].value->~ValueT();
      AllocatorValueT().deallocate(map_[i].value);
    }
  }
}
