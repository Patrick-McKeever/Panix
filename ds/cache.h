#ifndef VARDAROS_CACHE_H
#define VARDAROS_CACHE_H

#include <ds/optional.h>
#include <ds/hash_map.h>

namespace ds {
template <typename key_t, typename val_t>
class Cache
{
public:
    virtual bool Insert(const key_t &key, const val_t& entry) = 0;
    virtual ds::Optional<val_t> Lookup(const key_t &key)      = 0;
    /**
     *
     * @param num_entries Number of entries to evict.
     * @return The number of entries actually evicted. This may be smaller than
     *         the given number in the case where there are fewer entries in
     *         the cache than were requested to be evicted.
     */
    virtual size_t Evict(size_t num_entries) = 0;
    virtual void Flush() = 0;
    virtual size_t NumEntries() const        = 0;
};

template <typename key_t, typename val_t, typename allocator_t=KernelAllocator>
class LRUCache : public Cache<key_t, val_t>
{
public:
    LRUCache(void (*eviction_handler)(key_t, val_t) = nullptr)
        : mru_(nullptr)
        , lru_(nullptr)
        , eviction_handler_(eviction_handler)
        , num_entries_(0)
    {}

    ~LRUCache()
    {
        Evict(num_entries_);
    }

    bool Insert(const key_t &key, const val_t &val) override
    {
        if(BuddyAllocator::MemCritical()) {
            Evict(1);
        }

        auto new_entry =
            (LinkedListEntry*) allocator_t::Allocate(sizeof(LinkedListEntry));
        new_entry->key_  = key;
        new_entry->val_  = val;
        new_entry->prev_ = nullptr;
        new_entry->next_ = mru_;
        if(new_entry->next_) {
            new_entry->next_->prev_ = new_entry;
        }

        if (! entries_.Insert(key, new_entry)) {
            return false;
        }
        mru_ = new_entry;
        if (lru_ == nullptr) {
            lru_ = new_entry;
        }
        ++num_entries_;
        return true;
    }

    virtual ds::Optional<val_t> Lookup(const key_t& key) override
    {
        if (ds::Optional<LinkedListEntry*> opt_entry = entries_.Lookup(key)) {
            LinkedListEntry* entry = opt_entry.Value();
            if (entry->prev_) {
                entry->prev_->next_ = entry->next_;
            }
            if (entry->next_) {
                entry->next_->prev_ = entry->prev_;
            }
            if (num_entries_ > 1 && lru_ == entry) {
                lru_ = entry->prev_;
            }

            entry->prev_ = nullptr;
            if (mru_ != entry) {
                entry->next_ = mru_;
                mru_         = entry;
            }

            if(entry->next_) {
                entry->next_->prev_ = entry;
            }
            return entry->val_;
        }
        return ds::NullOpt;
    }

    virtual size_t Evict(size_t entries_to_evict) override
    {
        size_t i;
        for (i = 0; i < entries_to_evict && num_entries_; ++i) {
            LinkedListEntry* entry_to_evict = lru_;
            lru_                            = lru_->prev_;
            if (lru_) {
                lru_->next_ = nullptr;
            }

            entries_.Delete(entry_to_evict->key_);
            --num_entries_;

            // The eviction handler is useful in cases where the object requires
            // some cleanup, e.g. if val_t is a pointer.
            if (eviction_handler_) {
                eviction_handler_(entry_to_evict->key_, entry_to_evict->val_);
            }

            allocator_t::Free(entry_to_evict);
        }

        return i;
    }

    virtual void Flush() override
    {
        Evict(num_entries_);
    }

    virtual size_t NumEntries() const override
    {
        return num_entries_;
    }

private:
    struct LinkedListEntry
    {
        key_t key_;
        val_t val_;
        LinkedListEntry *prev_, *next_;
    };

    LinkedListEntry *mru_, *lru_;
    void (*eviction_handler_)(key_t key, val_t val);
    ds::HashMap<key_t, LinkedListEntry*, allocator_t> entries_;
    size_t num_entries_;
};

}

#endif
