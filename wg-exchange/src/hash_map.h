#pragma once

#include <boost/predef/architecture.h>

#include <atomic>
#include <memory>
#include <new>
#include <optional>
#include <tuple>

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

/**
 * Basic attempt at creating lock-free hashmap.
 * insert, update and read (no deletion, since its not needed here).
 *
 * update is NOT lock-free. the update itself blocks other updates. (reads can
 * continue on and fail spuriously when an update is happening)
 *
 * NOTE: spriously fails at reads with optional result
 * TODO: Testing, Benchmark this wrt mutex
 */
template <typename KeyT, typename ValueT, typename HashFn, typename ProbeFn,
          typename EqualityKeyFn = std::equal_to<KeyT>, typename AllocatorValueT = std::allocator<ValueT>>
class FixedSizeLockFreeHashMap
{
  public:
    static_assert(std::is_copy_assignable<KeyT>::value && std::is_move_assignable<KeyT>::value,
                  "KeyT needs to be copy and move assignable");
    static_assert(std::is_copy_assignable<ValueT>::value && std::is_move_assignable<ValueT>::value,
                  "ValueT needs to be copy and move assignable");
    static_assert(std::is_invocable_r<size_t, HashFn, const KeyT &>::value, "Invalid HashFn");
    static_assert(std::is_invocable_r<size_t, ProbeFn, size_t, size_t>::value, "Invalid ProbeFn");
    static_assert(std::is_invocable_r<bool, EqualityKeyFn, const KeyT &, const KeyT &>::value, "Invalid EqualityKeyFn");

    typedef std::pair<const KeyT, ValueT> ValuePair;
    typedef std::optional<ValuePair> OptionalValuePair;

    class Deleter
    {
        void operator()(FixedSizeLockFreeHashMap *ptr)
        {
            destroy(ptr);
        }
    };

    typedef std::unique_ptr<FixedSizeLockFreeHashMap, Deleter> SomePointer;

    // Iterator always return a copy of the data on operator* and operator->
    // as an OptionalValuePair since failures are possible. It will be set weak by
    // default and will always be forward_iterator for simplicity.
    class Iterator
    {
      public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = OptionalValuePair;
        // Not required, this is forward iterator only
        using difference_type = size_t;
        // Returning new instance whether pointer or reference
        // don't know if this will create problems somewhere
        // it shouldn't since its not random_access_tag
        using pointer = OptionalValuePair;
        using reference = OptionalValuePair;

        Iterator(FixedSizeLockFreeHashMap *m, size_t i, bool w = true) : hmap_(m), idx_(i), weak_(w)
        {
        }
        Iterator() : Iterator(nullptr, -1);

        // mostly to differentiate end() and invalid()
        // discard weak comparison
        bool operator==(const Iterator &b)
        {
            return ((hmap_ == b.hmap_) && idx_ == b.idx_);
        }
        bool operator!=(const Iterator &b)
        {
            return !(*this == b);
        }
        // ++it
        Iterator &operator++()
        {
            idx_++;
            skip_empty();
            return *this;
        }
        // it++
        Iterator operator++(int)
        {
            Iterator it = *this;
            idx_++;
            skip_empty();
            return it;
        }
        reference operator*()
        {
            return hmap_->get_value_pair(idx_, weak_);
        }
        pointer operator->()
        {
            return hmap_->get_value_pair(idx_, weak_);
        }

      protected:
        void skip_empty()
        {
            for (; idx_ < hmap_->capacity_ && !hmap_->isNotEmpty(idx_); idx_++)
                ;
        }

      private:
        // Raw pointer... bad stuff
        FixedSizeLockFreeHashMap *hmap_;
        size_t idx_;
        bool weak_;
    };

    static const size_t kcapacity_max = 512 - 1;

    static void destroy(FixedSizeLockFreeHashMap *ptr);
    static SomePointer create(int capacity = kcapacity_max);

    // If the bool is false, and iterator == end() then capacity limit reached
    // If the bool is false, and iterator != end() then contention issue
    std::pair<Iterator, bool> insert(ValuePair &&value_pair)
    {
        size_t idx = insert_internal(std::move(value_pair));
        return std::make_pair(Iterator(this, idx), idx >= 0 && idx < capacity_);
    }
    std::pair<Iterator, bool> update(ValuePair &&value_pair)
    {
        size_t idx = update_internal(std::move(value_pair));
        return std::make_pair(Iterator(this, idx), idx >= 0 && idx < capacity_);
    }

    // Weak - Spuriously fails on version mismatch.
    // Strong - Very expensive, spins till version mismatch is resolved or
    // spin counter goes beyond the limit
    Iterator find(const KeyT &fkey, bool weak = true)
    {
        return Iterator(this, find_internal(fkey), weak);
    }

    Iterator begin()
    {
        Iterator it(this, 0);
        it.skip_empty();
        return it;
    }

    // just to differentiate between insert/update spurious failures from failures
    // on max capacity reaching
    constexpr Iterator end()
    {
        return Iterator(this, capacity_);
    }
    constexpr Iterator invalid()
    {
        return Iterator(this, -1);
    }

  protected:
    OptionalValuePair get_value_pair(size_t, bool = true);
    bool isNotEmpty(size_t);

  private:
    // version - resolves ABA issue, state for inserting
    // 2 byte should be applicable for lock free atomic (__atomic_is_lock_free)
    // based on cpu-arch
    class Track
    {
        uint8_t state;
        uint8_t version;
        static const uint8_t kempty_state = 1;
        static const uint8_t kfilled_state = 1 << 1;
        static const uint8_t klocked_state = 1 << 2;
        static const uint8_t kdestroyed_state = 1 << 3;
    } class Entry
    {
        KeyT key;
        ValueT *value;
        // Force the alignment of atomic onto a separate cacheline to prevent false
        // sharing.
        alignas(std::hardware_destructive_interference_size) std::atomic<Track> track;
    };

    size_t capacity_;
    alignas(std::hardware_destructive_interference_size) std::atomic<size_t> size_;
    // Zero length array needs to be at the end
    Entry map_[0];

    // Alignment/Size issues don't arise because alignas was only applied to
    // member types of the classes. Compiler makes sure that the classes
    // themselves have correct alignment.
    // i.e. alignment <= size && size % alignment == 0
    static constexpr size_t get_final_aligned_size(const size_t &capacity)
    {
        return sizeof(FixedSizeLockFreeHashMap) + sizeof(Entry) * capacity;
    }
    size_t find_internal(const KeyT &);
    size_t insert_internal(ValuePair &&);
    size_t update_internal(ValuePair &&);
    FixedSizeLockFreeHashMap() = delete;
    FixedSizeLockFreeHashMap(size_t capacity);
    ~FixedSizeLockFreeHashMap();
};
