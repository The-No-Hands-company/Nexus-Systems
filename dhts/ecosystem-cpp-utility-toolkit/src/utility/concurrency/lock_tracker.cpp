#include "nexus/utility/concurrency/lock_tracker.h"
#include <algorithm>
namespace nexus::utility::concurrency {
LockTracker& LockTracker::instance(){static LockTracker t;return t;}
void LockTracker::onBeforeLock(const void* m){std::unique_lock l(mutex_);waiting_for_[std::this_thread::get_id()]=m;}
void LockTracker::onAfterLock(const void* m){std::unique_lock l(mutex_);auto tid=std::this_thread::get_id();waiting_for_.erase(tid);held_locks_[tid].push_back(m);lock_owners_[m]=tid;}
void LockTracker::onUnlock(const void* m){std::unique_lock l(mutex_);auto tid=std::this_thread::get_id();auto it=held_locks_.find(tid);if(it!=held_locks_.end()){auto&lks=it->second;lks.erase(std::remove(lks.begin(),lks.end(),m),lks.end());if(lks.empty())held_locks_.erase(it);}lock_owners_.erase(m);}
LockTracker::HeldLocksMap LockTracker::getSnapshotHeldLocks()const{std::shared_lock l(mutex_);return held_locks_;}
LockTracker::WaitingMap LockTracker::getSnapshotWaiting()const{std::shared_lock l(mutex_);return waiting_for_;}
} // namespace nexus::utility::concurrency
