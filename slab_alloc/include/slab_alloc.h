#if !defined(SLAB_ALLOC_H)

#include <unordered_map>
#include <memory>
#include <algorithm>
#include <cassert>
#include <cstdint>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#define SLAB_ALLOC_DEBUG

namespace hft
{
    // TODO(vss): Double free is not detected and causes silent corruption
    // No thread safety
    // Empty slabs are never returned to OS
    // No bounds checking on deallocate(assumes valid slab pointers)

    class SlabAlloc
    {
        struct Cache;
        struct Slab
        {
            Slab *next;
            Slab *prev;
            Cache *owner;
            size_t total_slots;
            size_t free_slots;
            void *free_list;
        };
    
        struct Cache
        {
            Cache(size_t obj_size, size_t slab_size)
                : obj_size(obj_size)
                , slab_size(slab_size)
                , partial(nullptr)
                , full(nullptr)
            {}

            size_t obj_size;
            size_t slab_size;
            Slab *partial;
            Slab *full;
        };
        
    public:

        SlabAlloc()
        {
            m_default_slab_size = 4096;
        }

        ~SlabAlloc()
        {
            for (auto &entry : m_cache)
            {
                Cache *cache = entry.second.get();
                if (!cache) continue;

                auto free_list = [] (Slab *head)
                    {
                        Slab *slab = head;
                        while (slab)
                        {
                            Slab *next = slab->next;
                            VirtualFree(reinterpret_cast<void *>(slab), 0, MEM_RELEASE);
                            slab = next;
                        }
                    };

                free_list(cache->partial);
                free_list(cache->full);
            }

            m_cache.clear();
        }

#ifdef SLAB_ALLOC_DEBUG
        size_t DebugAlignedSize(size_t bytes) const noexcept
        {
            auto aligned = AlignUp(bytes, sizeof(void *));
            return std::max(aligned, sizeof(void *));
        }

        size_t DebugSlotsPerSlab(size_t obj_size) const noexcept
        {
            auto aligned = DebugAlignedSize(obj_size);
            auto* cache = FindCache(aligned);
            if (!cache)
            {
                size_t header_size = AlignUp(sizeof(Slab), sizeof(void *));
                size_t usable = m_default_slab_size - header_size;
                return usable / aligned;
            }

            Slab *slab = cache->partial ? cache->partial : cache->full;
            return slab ? slab->total_slots : 0;
        }

        Slab *DebugSlabHeaderFromPtr(void *p) noexcept
        {
            if (!p) return nullptr;
            
            uintptr_t x = reinterpret_cast<uintptr_t>(p);
            uintptr_t base = x & ~(static_cast<uintptr_t>(m_default_slab_size) - 1);
            
            if (base % m_default_slab_size != 0) return nullptr;

            Slab *slab = reinterpret_cast<Slab *>(base);

            if (!slab) return nullptr;
            if (!slab->owner) return nullptr;
            
            return slab;
        }

        size_t DebugSlabsInCache(size_t obj_size) noexcept
        {
            auto aligned = DebugAlignedSize(obj_size);
            auto* cache = FindCache(aligned);
            if (!cache) return 0;

            auto count_list = [] (const Slab *head) -> size_t 
                {
                    size_t n = 0;
                    const Slab *cur = head;
                    while (cur)
                    {
                        ++n;
                        cur = cur->next;
                    }
                    return n;
                };

            size_t result = count_list(cache->partial) + count_list(cache->full);
            return result;
        }
#endif

        void *Allocate(uint64_t bytes)
        {
            if (bytes == 0)
            {
                return nullptr;
            }

            size_t aligned_size = AlignUp(bytes, sizeof(void *));
            aligned_size = std::max(aligned_size, sizeof(void *));

            Cache *cache = FindCache(aligned_size);
            if (!cache)
            {
                cache = CreateCache(aligned_size);
            }

            if (!cache->partial)
            {
                Slab *slab = CreateSlab(cache);
                if (!slab)
                {
                    return nullptr;
                }

                InsertSlabIntoList(&cache->partial, slab);
            }

            Slab *slab = cache->partial;
            void *obj = PopFromSlab(slab);
            assert(obj != nullptr);

            if (!slab->free_slots)
            {
                RemoveSlabFromList(&cache->partial, slab);
                InsertSlabIntoList(&cache->full, slab);
            }

            return obj;
        }   
        
        void Deallocate(void *p)
        {
            if (!p)
            {
                return;
            }

            size_t slab_size = m_default_slab_size;
            uintptr_t x = reinterpret_cast<uintptr_t>(p);
            uintptr_t base = x & ~(slab_size - 1);
            Slab *slab = reinterpret_cast<Slab *>(base);
            
            assert(slab->owner != nullptr);
            Cache *cache = slab->owner;
            bool was_full = (slab->free_slots == 0);
            
            PushToSlab(slab, p);

            if (was_full)
            {
                RemoveSlabFromList(&cache->full, slab);
                InsertSlabIntoList(&cache->partial, slab);
            }

            // TODO(vss): Handle completely empty slabs so we dont keep them forever,
            // simple policy could be: if slab->free_slots == slab->total_slots
            // keep N empty slabs cached or VirtualFree them all
        }

    private:
        size_t m_default_slab_size = 4096;
        std::unordered_map<size_t, std::unique_ptr<Cache>> m_cache;

        static inline size_t AlignUp(size_t n, size_t a) { return (n + (a - 1)) & ~(a - 1); }

        Cache *FindCache(size_t obj_size) const
        {
            auto it = m_cache.find(obj_size);
            return (it != m_cache.end()) ? it->second.get() : nullptr;
        }

        Cache *CreateCache(size_t obj_size)
        {
            auto [it, _] = m_cache.emplace(
                obj_size, std::make_unique<Cache>(obj_size, m_default_slab_size));
            return it->second.get();
        }

        Slab *CreateSlab(Cache *cache)
        {
            size_t slab_size = cache->slab_size;
            void *memory = VirtualAlloc(nullptr, slab_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            if (!memory) 
            { 
                return nullptr;
            }
            
            Slab *slab = reinterpret_cast<Slab *>(memory);
            slab->prev = nullptr;
            slab->next = nullptr;
            slab->owner = cache;

            size_t header_size = AlignUp(sizeof(Slab), sizeof(void *));
            size_t usable = slab_size - header_size;
            size_t obj_size = cache->obj_size;
            size_t slots = usable / obj_size;
            if (slots == 0)
            {
                VirtualFree(memory, 0, MEM_RELEASE);
                return nullptr;
            }
            slab->total_slots = slots;
            slab->free_slots = slots;

            char *cursor = reinterpret_cast<char *>(memory) + header_size;
            slab->free_list = cursor;
            for (size_t i = 0; i < slots - 1; ++i)
            {
                void *next = cursor + obj_size;
                *reinterpret_cast<void **>(cursor) = next;
                cursor += obj_size;
            }

            *reinterpret_cast<void **>(cursor) = nullptr;
            return slab;
        }

        static inline void InsertSlabIntoList(Slab **head, Slab *slab)
        {
            slab->next = *head;
            slab->prev = nullptr;
            
            if (*head)
            {
                (*head)->prev = slab;
            }

            *head = slab;
        }
        
        static inline void RemoveSlabFromList(Slab **head, Slab *slab)
        {
            if (slab->prev) 
            { 
                slab->prev->next = slab->next; 
            }

            if (slab->next) 
            { 
                slab->next->prev = slab->prev; 
            }

            if (slab == *head) 
            {
                *head = slab->next; 
            }

            slab->next = nullptr;
            slab->prev = nullptr;
        }

        inline void *PopFromSlab(Slab *slab)
        {
            if (!slab->free_list)
            {
                return nullptr;
            }

            void *obj = slab->free_list;
            slab->free_list = *reinterpret_cast<void **>(obj);
            --slab->free_slots;
            return obj;
        }

        inline void PushToSlab(Slab *slab, void *obj)
        {
            *reinterpret_cast<void **>(obj) = slab->free_list;
            slab->free_list = obj;
            ++slab->free_slots;
        }

    };
} // namespace hft
#define SLAB_ALLOC_H
#endif