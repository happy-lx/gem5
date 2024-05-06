/**
 * @file
 * Stream Prefetcher template instantiations.
 */

#include "mem/cache/prefetch/stream.hh"

#include <cassert>

#include "base/intmath.hh"
#include "base/logging.hh"
#include "base/random.hh"
#include "base/trace.hh"
#include "debug/HWPrefetch.hh"
#include "debug/StreamFlag.hh"
#include "mem/cache/prefetch/associative_set_impl.hh"
#include "mem/cache/replacement_policies/base.hh"
#include "params/StreamPrefetcher.hh"

namespace gem5
{

namespace prefetch
{

Stream::Stream(const StreamPrefetcherParams &p)
  : Queued(p),
    regionSize(p.region_size),
    activeSizeThreshold(p.region_size * p.active_threshold/100.0),
    degree(p.degree),
    distance(p.distance)
{
    srand(time(0));
    for(int i = 0; i < p.bit_vector_entry; i++)
        bitVector.emplace_back(0, regionSize, blkSize, activeSizeThreshold);
    for(int i = 0; i < p.bit_vector_entry; i++)
        bitVector[i].setId(i);
    DPRINTF(StreamFlag, "Stream Prefetcher, regionSize: %d \
    , activeSizeThreshold: %d\n", regionSize, activeSizeThreshold);
}

Stream::BitVectorEntry::BitVectorEntry(const Addr& addr, const uint64_t regionSize, const uint64_t blockSize, const uint64_t activeSizeThreshold)
  : regionTag(getRegionTag(addr)),
    active(false),
    decr(false),
    _regionSize(regionSize),
    _blockSize(blockSize),
    _threshold(activeSizeThreshold),
    _debug_id(0)
{
    for(int i = 0; i < getRegionCnts(); i++)
        regionBit.push_back(false);

    DPRINTF(StreamFlag, "Init BitVectorEntry, regionBit size: %d\n", regionBit.size());
    invalidate();
}

void
Stream::BitVectorEntry::newEntry(const Addr& addr, bool act, bool dec)
{
    invalidate();
    regionTag = getRegionTag(addr);
    active = act;
    decr = dec;
    regionBit[getRegionBits(addr)] = true;

    DPRINTF(StreamFlag, "New A Stream Entry, Region Tag: 0x%x, Trigger Addr: 0x%x\n", regionTag, addr);
}

void
Stream::BitVectorEntry::accessEntry(const Addr& addr)
{
    regionBit[getRegionBits(addr)] = true;
    active = active || (getValidCnt() > getActThreshCnt());

    DPRINTF(StreamFlag, "Accessing A Stream Entry, Region Tag: 0x%x, Trigger Addr: 0x%x, Valid Region Bit: %d\n", regionTag, addr, getValidCnt());
}

bool
Stream::BitVectorEntry::canTriggerPrefetch(const Addr& addr)
{
    DPRINTF(StreamFlag, "Checking Pf Condition, Trigger Addr: 0x%x, active: %d\n", addr, active);
    return active && !regionBit[getRegionBits(addr)];
}

Stream::BitVectorEntry*
Stream::getMatchEntry(const Addr& addr)
{
    for(int i = 0; i < bitVector.size(); i++) {
        if(bitVector[i].match(addr)) {
            DPRINTF(StreamFlag, "Found A Match Entry, Trigger Addr: 0x%x\n", addr);
            return &(bitVector[i]);
        }
    }
    DPRINTF(StreamFlag, "Can Not Found A Match Entry, Trigger Addr: 0x%x\n", addr);
    return nullptr;
}

Stream::BitVectorEntry*
Stream::getVictimEntry(const Addr& addr)
{
    // for simple, use random replacement first
    static uint64_t rnd = 0;
    rnd = rand() % bitVector.size();
    DPRINTF(StreamFlag, "Try Finding a Victim Entry, Trigger Addr: 0x%x, Found entry %d\n", addr, rnd);
    return &(bitVector[rnd]);
}

void
Stream::calculatePrefetch(const PrefetchInfo &pfi,
                                    std::vector<AddrPriority> &addresses,
                                    const CacheAccessor &cache)
{
    // Get required packet info
    Addr pf_addr = pfi.getAddr();

    // Search for entry inside BitVectors
    Stream::BitVectorEntry* curRegionEntry = getMatchEntry(pf_addr);
    Stream::BitVectorEntry* prevRegionEntry = getMatchEntry(pf_addr - regionSize);
    Stream::BitVectorEntry* nextRegionEntry = getMatchEntry(pf_addr + regionSize);

    if(curRegionEntry) {
        if(curRegionEntry->canTriggerPrefetch(pf_addr)) {
            if(curRegionEntry->decr) {
                Addr new_pf_addr = pf_addr - distance * blkSize;
                // Generate up to degree prefetches
                for (int d = 0; d < degree; d++) {
                    DPRINTF(StreamFlag, "Issue Prefetch, PF Addr: 0x%x\n", new_pf_addr);
                    addresses.push_back(AddrPriority(new_pf_addr, 0));
                    new_pf_addr -= blkSize;
                }
            }else {
                Addr new_pf_addr = pf_addr + distance * blkSize;
                // Generate up to degree prefetches
                for (int d = 0; d < degree; d++) {
                    DPRINTF(StreamFlag, "Issue Prefetch, PF Addr: 0x%x\n", new_pf_addr);
                    addresses.push_back(AddrPriority(new_pf_addr, 0));
                    new_pf_addr += blkSize;
                }
            }
        }
        curRegionEntry->accessEntry(pf_addr);
    }else {
        // miss in BitVectors
        Stream::BitVectorEntry* vicRegionEntry = getVictimEntry(pf_addr);
        bool alloc_active = false;

        if(prevRegionEntry && prevRegionEntry->active) {
            alloc_active = true;
        }else if(nextRegionEntry && nextRegionEntry->active) {
            alloc_active = true;
        }
        
        vicRegionEntry->newEntry(pf_addr, alloc_active, nextRegionEntry ? true : false);
    }
}

} // namespace prefetch
} // namespace gem5
