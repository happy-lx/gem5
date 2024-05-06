/**
 * @file
 * Describes a Stream prefetcher.
 */

#ifndef __MEM_CACHE_PREFETCH_STREAM_HH__
#define __MEM_CACHE_PREFETCH_STREAM_HH__

#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>

#include "base/sat_counter.hh"
#include "base/types.hh"
#include "mem/cache/prefetch/associative_set.hh"
#include "mem/cache/prefetch/queued.hh"
#include "mem/cache/replacement_policies/replaceable_entry.hh"
#include "mem/cache/tags/indexing_policies/set_associative.hh"
#include "mem/packet.hh"
#include "params/StreamPrefetcher.hh"
#include "params/StridePrefetcherHashedSetAssociative.hh"

namespace gem5
{
struct StreamPrefetcherParams;

namespace prefetch
{

class Stream : public Queued
{
  protected:

    const uint64_t regionSize;

    const uint64_t activeSizeThreshold;

    const int degree;

    const int distance;

  public:

    class BitVectorEntry
    {
      public:

        Addr regionTag;
        bool active;
        bool decr;

      private:
        uint64_t _regionSize;
        uint64_t _blockSize;
        uint64_t _threshold;
        uint64_t _debug_id;
        std::vector<bool> regionBit;

      public:
        inline Addr getBlockAddr(const Addr& addr) const
        {
          return addr >> static_cast<uint64_t>(log2(_blockSize));
        }
        inline Addr getRegionTag(const Addr& addr) const
        {
          return addr >> static_cast<uint64_t>(log2(_regionSize));
        }
        inline Addr getRegionBits(const Addr& addr) const
        {
          return getBlockAddr(addr & (_regionSize - 1));
        }
        inline Addr getRegionCnts() const
        {
          return _regionSize / _blockSize;
        }
        inline Addr getActThreshCnt() const
        {
          return _threshold / _blockSize;
        }
        inline uint64_t getValidCnt() const
        {
          return std::accumulate(regionBit.begin(), regionBit.end(), 0, [](int x, bool t) -> int {return x + (t ? 1 : 0);});
        }
        inline void setId(const uint64_t id)
        {
          _debug_id = id;
        }

        BitVectorEntry(const Addr& addr, const uint64_t regionSize, const uint64_t blockSize, const uint64_t activeSizeThreshold);

        void invalidate()
        {
          // std::for_each(regionBit.begin(), regionBit.end(), [](bool& x){x = false;});
          for(int i = 0; i < regionBit.size(); i++) {
            regionBit[i] = false;
          }
          regionTag = 0;
          active = false;
        }
        void newEntry(const Addr& addr, bool act, bool dec);
        void accessEntry(const Addr& addr);
        bool canTriggerPrefetch(const Addr& addr);
        bool match(const Addr& addr) const {
          return regionTag == getRegionTag(addr);
        }

        const std::string
        name()
        {
            static const std::string default_name("BitVector");

            return default_name + std::to_string(_debug_id);
        }
    };

  private:
  
    std::vector<BitVectorEntry> bitVector;

    BitVectorEntry* getMatchEntry(const Addr& addr);
    BitVectorEntry* getVictimEntry(const Addr& addr);

  public:

    Stream(const StreamPrefetcherParams &p);

    void calculatePrefetch(const PrefetchInfo &pfi,
                           std::vector<AddrPriority> &addresses,
                           const CacheAccessor &cache) override;
};

} // namespace prefetch
} // namespace gem5

#endif // __MEM_CACHE_PREFETCH_STREAM_HH__
