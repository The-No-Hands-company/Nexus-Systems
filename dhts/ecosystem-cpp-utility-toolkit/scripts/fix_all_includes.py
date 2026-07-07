#!/usr/bin/env python3
import os, re
INC='include/nexus/utility'
m={
'std::chrono':'chrono','std::set<':'set','std::mutex':'mutex','std::lock_guard':'mutex',
'std::unordered_map':'unordered_map','std::unordered_set':'unordered_set',
'std::vector<':'vector','std::array<':'array','std::function<':'functional',
'std::atomic<':'atomic','std::thread':'thread','std::stacktrace':'stacktrace',
'std::source_location':'source_location','std::any':'any','std::optional<':'optional',
'std::regex':'regex','std::ofstream':'fstream','std::ifstream':'fstream',
'std::ostream':'ostream','std::istream':'istream',
'std::shared_ptr':'memory','std::unique_ptr':'memory','std::make_shared':'memory','std::make_unique':'memory',
'std::filesystem':'filesystem',
}
c=0
for r,_,fs in os.walk(INC):
 for f in fs:
  if not f.endswith('.h'): continue
  p=os.path.join(r,f)
  with open(p) as fh: cnt=fh.read()
  lines=cnt.split('\n')
  have=set()
  for l in lines:
   m2=re.match(r'\s*#include\s+<(\w+)>',l)
   if m2: have.add(m2.group(1))
  need=set()
  for pat,inc in m.items():
   if not inc: continue
   if pat in cnt and inc not in have:
    need.add(inc)
  if need:
   last=0
   for i,l in enumerate(lines):
    if l.strip().startswith('#include'): last=i
   for inc in sorted(need):
    lines.insert(last+1,f'#include <{inc}>')
    last+=1; c+=1
   with open(p,'w') as fh: fh.write('\n'.join(lines))
print(f'Added {c} includes')
