#include "nexus/utility/memory/memory_tracker.h"
#include <algorithm>
namespace nexus::utility::memory {
AllocationStackTrace::AllocationStackTrace(void* a,size_t s,const char* t):trace(nexus::utility::stacktrace::current()),size(s),address(a),type_name(t){}
void MemoryStats::recordAllocation(size_t s){++total_allocations;++current_allocations;total_bytes_allocated+=s;current_bytes+=s;if(current_allocations>peak_allocations)peak_allocations=current_allocations;if(current_bytes>peak_bytes)peak_bytes=current_bytes;}
void MemoryStats::recordDeallocation(size_t s){++total_deallocations;--current_allocations;total_bytes_deallocated+=s;current_bytes-=s;}
MemoryTracker& MemoryTracker::instance(){static MemoryTracker t;return t;}
MemoryTracker::~MemoryTracker(){auto l=detectLeaks();if(!l.empty())std::cerr<<"\n[MEMORY TRACKER] "<<l.size()<<" leak(s)!\n";}
void MemoryTracker::configure(const Config& c){std::lock_guard<std::mutex> lk(mutex_);config_=c;}
MemoryTracker::Config MemoryTracker::getConfig()const{std::lock_guard<std::mutex> lk(mutex_);return config_;}
void MemoryTracker::recordAllocation(void* p,size_t s,const char* tn){if(!p||!config_.track_allocations)return;std::lock_guard<std::mutex> lk(mutex_);stats_.recordAllocation(s);allocations_[p]=AllocationStackTrace(p,s,tn);}
void MemoryTracker::recordDeallocation(void* p){if(!p||!config_.track_allocations)return;std::lock_guard<std::mutex> lk(mutex_);auto it=allocations_.find(p);if(it!=allocations_.end()){stats_.recordDeallocation(it->second.size);allocations_.erase(it);}}
MemoryStats MemoryTracker::getStats()const{std::lock_guard<std::mutex> lk(mutex_);return stats_;}
std::vector<AllocationStackTrace> MemoryTracker::detectLeaks()const{std::lock_guard<std::mutex> lk(mutex_);std::vector<AllocationStackTrace> lks;for(auto&[_,i]:allocations_)lks.push_back(i);return lks;}
void MemoryTracker::printReport(std::ostream& os)const{std::lock_guard<std::mutex> lk(mutex_);os<<"\n=== Memory Report ===\n  Allocs:"<<stats_.total_allocations<<"\n  Current:"<<stats_.current_allocations<<"\n  Bytes:"<<stats_.current_bytes<<"\n======================\n";}
void MemoryTracker::reset(){std::lock_guard<std::mutex> lk(mutex_);allocations_.clear();stats_=MemoryStats{};}
void MemoryTracker::setEnabled(bool e){std::lock_guard<std::mutex> lk(mutex_);config_.track_allocations=e;}
bool MemoryTracker::isEnabled()const{std::lock_guard<std::mutex> lk(mutex_);return config_.track_allocations;}
MemoryTrackingScope::MemoryTrackingScope(bool e):previous_state_(MemoryTracker::instance().isEnabled()){MemoryTracker::instance().setEnabled(e);}
MemoryTrackingScope::~MemoryTrackingScope(){MemoryTracker::instance().setEnabled(previous_state_);}
} // namespace nexus::utility::memory
