#ifndef HASH_MAP_H
#define HASH_MAP_H

#include <ds/string.h>
#include <ds/optional.h>
#include <libc/string.h>
#include <stdint.h>
#include <stddef.h>

namespace ds {
class MurmurHasher
{
public:
    template <typename T>
    static uint32_t Hash(const T& plaintext)
    {
        return murmur3_32((uint8_t*) &plaintext, sizeof(plaintext), 0);
    }

    static uint32_t Hash(const ds::String &plaintext)
    {
        return murmur3_32((const uint8_t*) plaintext.ToChars(), plaintext.Len(),
                          0);
    }

private:
    static inline uint32_t murmur_32_scramble(uint32_t k)
    {
        k *= 0xcc9e2d51;
        k = (k << 15) | (k >> 17);
        k *= 0x1b873593;
        return k;
    }

    static uint32_t murmur3_32(const uint8_t* key, size_t len, uint32_t seed)
    {
        uint32_t h = seed;
        uint32_t k;
        for (size_t i = len >> 2; i; i--) {
            memcpy(&k, key, sizeof(uint32_t));
            key += sizeof(uint32_t);
            h ^= murmur_32_scramble(k);
            h = (h << 13) | (h >> 19);
            h = h * 5 + 0xe6546b64;
        }

        k = 0;
        for (size_t i = len & 3; i; i--) {
            k <<= 8;
            k |= key[i - 1];
        }

        h ^= murmur_32_scramble(k);
        h ^= len;
        h ^= h >> 16;
        h *= 0x85ebca6b;
        h ^= h >> 13;
        h *= 0xc2b2ae35;
        h ^= h >> 16;
        return h;
    }
};

/**
 *
 * @tparam key_t
 * @tparam val_t
 * @tparam allocator
 * @tparam inv_load_factor Inverse of the load factor. Using this instead of an
 *                         actual load factor, because I don't want to pollute
 *                         kernel code with floating points. When num_entries /
 *                         capacity exceeds 1 / inv_load_factor, increase size
 *                         of array by inv_load_factor.
 */
template <typename key_t,
          typename val_t,
          typename allocator_t   = KernelAllocator,
          typename hasher        = MurmurHasher,
          size_t inv_load_factor = 3>
class HashMap
{
    enum entry_type_t {
        EMPTY   = -128,
        DELETED = -2
    };

    struct HashMapEntry {
        key_t key_;
        val_t val_;
    };

public:
    HashMap(size_t initial_size = 1)
        : mdata_((int8_t*) allocator_t::Allocate(initial_size))
        , capacity_(initial_size)
        , num_entries_(0)
        , map_((HashMapEntry*) allocator_t::Allocate(initial_size *
                                                     sizeof(HashMapEntry)))
    {
        for (size_t i = 0; i < initial_size; ++i) {
            mdata_[i] = EMPTY;
        }
    }

    HashMap(const HashMap &rhs)
        : mdata_((int8_t*) allocator_t::Allocate(rhs.capacity_))
        , capacity_(rhs.capacity_)
        , num_entries_(rhs.num_entries_)
        , map_((HashMapEntry*) allocator_t::Allocate(capacity_ *
                                                     sizeof(HashMapEntry)))
    {
        for(size_t i = 0; i < capacity_; ++i) {
            mdata_[i] = rhs.mdata_[i];
            if(rhs.mdata_[i] != EMPTY && rhs.mdata_[i] != DELETED) {
                new(&map_[i].key_) key_t(rhs.map_[i].key_);
                new(&map_[i].val_) val_t(rhs.map_[i].val_);
            }
        }
    }

    HashMap &operator =(const HashMap &rhs)
    {
        if(&rhs == this) {
            return *this;
        }
        if(mdata_) {
            allocator_t::Free(mdata_);
        }
        if(map_) {
            for(size_t i = 0; i < capacity_; ++i) {
                if(mdata_[i] != EMPTY && mdata_[i] != DELETED) {
                    map_[i].key_.~key_t();
                    map_[i].val_.~val_t();
                }
            }
            allocator_t::Free(map_);
        }

        mdata_ = (int8_t*) allocator_t::Allocate(rhs.capacity_);
        capacity_ = rhs.capacity_;
        num_entries_ = (rhs.num_entries_);
        map_ = (HashMapEntry*) allocator_t::Allocate(capacity_ *
                                                       sizeof(HashMapEntry));
        for(size_t i = 0; i < rhs.capacity_; ++i) {
            mdata_[i] = rhs.mdata_[i];
            if(rhs.mdata_[i] != EMPTY && rhs.mdata_[i] != DELETED) {
                new (&map_[i].key_) key_t(rhs.map_[i].key_);
                new (&map_[i].val_) val_t(rhs.map_[i].val_);
            }
        }

        return *this;
    }

    HashMap(HashMap &&rhs) noexcept
            : mdata_(rhs.mdata_)
            , capacity_(rhs.capacity_)
            , num_entries_(rhs.num_entries_)
            , map_(rhs.map_)
    {
        rhs.mdata_ = nullptr;
        rhs.capacity_ = 0;
        rhs.num_entries_ = 0;
        rhs.map_ = nullptr;
    }

    HashMap &operator=(HashMap &&rhs) noexcept
    {
        if(mdata_) {
            allocator_t::Free(mdata_);
        }
        if(map_) {
            for(size_t i = 0; i < capacity_; ++i) {
                if(mdata_[i] != EMPTY && mdata_[i] != DELETED) {
                    map_[i].key_.~key_t();
                    map_[i].val_.~val_t();
                }
            }
            allocator_t::Free(map_);
        }

        mdata_ = rhs.mdata_;
        capacity_ = rhs.capacity_;
        num_entries_ = rhs.num_entries_;
        map_ = rhs.map_;

        rhs.mdata_ = nullptr;
        rhs.capacity_ = 0;
        rhs.num_entries_ = 0;
        rhs.map_ = nullptr;
        return *this;
    }

    ~HashMap()
    {
        if(mdata_) {
            allocator_t::Free(mdata_);
        }
        if(map_) {
            for(size_t i = 0; i < capacity_; ++i) {
                if(mdata_[i] != EMPTY && mdata_[i] != DELETED) {
                    map_[i].key_.~key_t();
                    map_[i].val_.~val_t();
                }
            }
            allocator_t::Free(map_);
        }
    }

    bool Insert(const key_t& key, const val_t& val)
    {
        size_t   ind;
        uint32_t hash = hasher::Hash(key);
        for (ind = H1(hash) % capacity_;
             mdata_[ind] != EMPTY && mdata_[ind] != DELETED;
             ind = (ind + 1) % capacity_) {
            if (H2(hash) == mdata_[ind] && key == map_[ind].key_) {
                return false;
            }
        }

        mdata_[ind] = H2(hash);
        new (&map_[ind].val_) val_t(val);
        new (&map_[ind].key_) key_t(key);
        //map_[ind]   = (HashMapEntry){ key, val };

        if (++num_entries_ > capacity_ / inv_load_factor) {
            Rehash();
        }

        return true;
    }

    ds::Optional<val_t> Lookup(const key_t& key) const
    {
        int64_t key_ind = Find(key);
        if (key_ind == -1) {
            return ds::NullOpt;
        }
        return map_[key_ind].val_;
    }

    bool Contains(const key_t& key)
    {
        int64_t key_ind = Find(key);
        return key_ind != -1;
    }

    bool Delete(const key_t& key)
    {
        int64_t key_ind = Find(key);
        if (key_ind == -1) {
            return false;
        }
        mdata_[key_ind] = DELETED;
        return true;
    }

    size_t Len() const
    {
        return num_entries_;
    }

    val_t operator[](const key_t &key) const
    {
        int64_t key_ind = Find(key);
        return map_[key_ind];
    }

    val_t& operator[](const key_t &key)
    {
        int64_t key_ind = Find(key);
        return (val_t&) map_[key_ind].val_;
    }

    class Itr {
    public:
        Itr()
            : hmap_(nullptr)
            , ind_(-1)
        {}

        ds::Optional<key_t> Key() const
        {
            if(Valid()) {
                return hmap_->map_[ind_]->key_;
            }
            return ds::NullOpt;
        }

        ds::Optional<val_t> Val() const
        {
            if(Valid()) {
                return hmap_->map_[ind_]->val_;
            }
            return ds::NullOpt;
        }

        void SetVal(const val_t &new_val)
        {
            if(Valid()) {
                hmap_[ind_].val_.~val_t();
                new(&hmap_[ind_].val_) val_t(new_val);
            }
        }

        bool Forward()
        {
            do { ++ind_; } while(! Valid());
            return Valid();
        }

        bool Backward()
        {
            do { --ind_; } while(! Valid());
            return Valid();
        }

        bool Valid() const
        {
            return ind_ < hmap_->capacity_ && ind_ > 0 &&
                    hmap_->mdata_[ind_] != EMPTY &&
                    hmap_->mdata_[ind_] != DELETED;
        }

        void Reset()
        {
            ind_ = -1;
            Forward();
        }

        val_t operator *() const noexcept
        {
            return hmap_->map_[ind_].val_;
        }

        val_t* operator ->() const noexcept
        {
            if(Valid()) {
                return &hmap_->map_[ind_].val_;
            }
            return (val_t*) (nullptr);
        }

    private:
        Itr(HashMap *hmap, size_t ind)
            : hmap_(hmap)
            , ind_(ind)
        {}

        HashMap *hmap_;
        size_t ind_;
    };

    Itr Begin() const
    {
        Itr itr(this, -1);
        itr.Forward();
        return itr;
    }

    Itr End() const
    {
        Itr itr(this, capacity_);
        itr.Backward();
        return itr;
    }

private:
    int8_t *mdata_;
    size_t  capacity_, num_entries_;
    HashMapEntry *map_;

    int64_t Find(const key_t& key) const
    {
        uint32_t hash = hasher::Hash(key);

        for (uint32_t ind = H1(hash) % capacity_;
             ind < capacity_ && mdata_[ind] != EMPTY;
             ind = (ind + 1) % capacity_)
        {
            if (H2(hash) == mdata_[ind] && key == map_[ind].key_) {
                return ind;
            }
        }

        return -1;
    }

    void Rehash()
    {
        size_t old_capacity = capacity_;
        // Bit lazy, but this gives us a good chance of getting a mersennes
        // prime as a new capacity. Will implement some prime sieve later as
        // a replacement.
        capacity_       = (capacity_ + 1) * 2 - 1;
        auto* new_mdata = (int8_t*) allocator_t::Allocate(capacity_);
        auto* new_map   = (HashMapEntry*) allocator_t::Allocate(
            capacity_ * sizeof(HashMapEntry));

        for (uint32_t i = 0; i < capacity_; ++i) {
            new_mdata[i] = EMPTY;
        }

        for (uint32_t i = 0; i < old_capacity; ++i) {
            if (mdata_[i] != EMPTY && mdata_[i] != DELETED) {
                uint32_t hash = hasher::Hash(map_[i].key_);

                uint32_t new_ind;
                for (new_ind = H1(hash) % capacity_;
                     new_mdata[new_ind] != EMPTY;
                     new_ind = (new_ind + 1) % capacity_)
                    ;

                new_mdata[new_ind] = H2(hash);
                new (&new_map[new_ind].key_) key_t(map_[i].key_);
                new (&new_map[new_ind].val_) val_t(map_[i].val_);
            }
        }

        allocator_t::Free(mdata_);
        allocator_t::Free(map_);
        mdata_ = new_mdata;
        map_   = new_map;
    }

    static uint32_t H1(uint32_t hash)
    {
        return hash >> 7;
    }

    static int8_t H2(uint32_t hash)
    {
        return hash & 0x7F;
    }
};



}
#endif
