#ifndef GEM5_NEXTLINE_HH
#define GEM5_NEXTLINE_HH

#include <boost/circular_buffer.hpp>
#include <boost/compute/detail/lru_cache.hpp>

#include "base/types.hh"
#include "mem/cache/prefetch/associative_set.hh"
#include "mem/cache/prefetch/queued.hh"
#include "mem/cache/replacement_policies/replaceable_entry.hh"
#include "mem/cache/tags/indexing_policies/set_associative.hh"
#include "mem/packet.hh"
#include "params/CMCPrefetcher.hh"

namespace gem5
{
struct CMCPrefetcherParams;
GEM5_DEPRECATED_NAMESPACE(Prefetcher, prefetch);

namespace prefetch
{


class CMCPrefetcher : public Queued
{
  public:
    class StorageEntry;
    class RecordEntry
    {
        public:
            Addr pc;
            Addr addr;
            bool is_secure;
            RecordEntry(Addr p, Addr a, bool s)
                : pc(p), addr(a), is_secure(s) {}
            RecordEntry() : addr(0), is_secure(true) {}
    };
    class Recorder
    {
        public:
            std::vector<Addr> entries;
            int index;
            const int degree;
            Recorder(int d) : entries(), index(0), degree(d) {}
            bool entry_empty() { return entries.empty(); }
            Addr get_base_addr() { return entries[0]; }

            bool train_entry(Addr, bool, bool*);
            void reset();
            const int nr_entry = 16;
        private:
    };

    class StorageEntry : public TaggedEntry
    {
        public:
            std::vector<Addr> addresses;
            int refcnt;
            uint64_t id;
            void invalidate() override;
    };
  private:
    Recorder *recorder;
    AssociativeSet<StorageEntry> storage;
    const int degree;
    uint64_t acc_id = 1;

    bool enableDB;

  public:
    CMCPrefetcher(const CMCPrefetcherParams &p);
    void calculatePrefetch(const PrefetchInfo &pfi, std::vector<AddrPriority> &addresses, const CacheAccessor &cache) override
    {
        panic("not implemented");
    };

    boost::compute::detail::lru_cache<Addr, Addr> *filter;

    void doPrefetch(const PrefetchInfo &pfi, std::vector<AddrPriority> &addresses, bool late,
                           PrefetchSourceType pf_source, bool is_first_shot);
  private:
    uint64_t hash(Addr addr, Addr pc) {
        return addr ^ (pc<<8);
    }

    bool sendPFWithFilter(const PrefetchInfo &pfi, Addr addr, std::vector<AddrPriority> &addresses, int prio,
                          PrefetchSourceType src);

    static const int STACK_SIZE = 4;
    boost::circular_buffer<RecordEntry> trigger;
    // RecordEntry trigger_stack[STACK_SIZE];
};

}  // namespace prefetch
}  // namespace gem5

#endif  // GEM5_SMS_HH
