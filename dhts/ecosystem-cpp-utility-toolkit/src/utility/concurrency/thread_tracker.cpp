#include "nexus/utility/concurrency/thread_tracker.h"
#include <sstream>
namespace nexus::utility::concurrency {
ThreadTracker& ThreadTracker::instance(){static ThreadTracker t;return t;}
void ThreadTracker::registerThread(const std::string& n){std::unique_lock l(mutex_);threads_[std::this_thread::get_id()]=n;}
void ThreadTracker::unregisterThread(){std::unique_lock l(mutex_);threads_.erase(std::this_thread::get_id());}
std::optional<std::string> ThreadTracker::getThreadName(std::thread::id id)const{std::shared_lock l(mutex_);auto it=threads_.find(id);return it!=threads_.end()?std::optional(it->second):std::nullopt;}
std::string ThreadTracker::getCurrentThreadName()const{auto n=getThreadName(std::this_thread::get_id());if(n)return *n;std::ostringstream o;o<<"thread-"<<std::this_thread::get_id();return o.str();}
size_t ThreadTracker::getActiveThreadCount()const{std::shared_lock l(mutex_);return threads_.size();}
void ThreadTracker::printReport(std::ostream& os)const{std::shared_lock l(mutex_);os<<"\n=== Thread Report ===\n  Active: "<<threads_.size()<<"\n";for(auto&[id,name]:threads_)os<<"    "<<name<<"\n";os<<"=====================\n";}
} // namespace nexus::utility::concurrency
