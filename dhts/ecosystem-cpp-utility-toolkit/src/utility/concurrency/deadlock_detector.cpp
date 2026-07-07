#include "nexus/utility/concurrency/deadlock_detector.h"
#include <unordered_set>
namespace nexus::utility::concurrency {
std::optional<std::thread::id> DeadlockDetector::checkCycle(const std::unordered_map<std::thread::id,const void*>&wm,const std::unordered_map<const void*,std::thread::id>&lo){
std::unordered_map<std::thread::id,std::thread::id>g;for(auto&[t,m]:wm){auto oi=lo.find(m);if(oi!=lo.end())g[t]=oi->second;}
std::unordered_set<std::thread::id> vis,st;auto dfs=[&](auto&& self,std::thread::id cur)->bool{vis.insert(cur);st.insert(cur);auto it=g.find(cur);if(it!=g.end()){if(st.count(it->second))return true;if(!vis.count(it->second)&&self(self,it->second))return true;}st.erase(cur);return false;};
for(auto&[t,_]:wm)if(!vis.count(t)&&dfs(dfs,t))return t;return std::nullopt;}
} // namespace nexus::utility::concurrency
