#!/usr/bin/env python3
import os
SRC='src/utility'; INC='include/nexus/utility'
c=0
for r,_,fs in os.walk(SRC):
 for f in fs:
  if not f.endswith('.cpp'): continue
  p=os.path.join(r,f)
  with open(p) as fh: cpp=fh.read()
  if 'instance()' not in cpp: continue
  rel=os.path.relpath(p,SRC); cat=os.path.dirname(rel); name=os.path.splitext(os.path.basename(rel))[0]
  hp=os.path.join(INC,cat,name+'.h')
  if not os.path.exists(hp): continue
  if os.path.getsize(hp)>500:
   with open(hp) as fh: h=fh.read()
   if 'instance()' not in h:
    nc='// Header-only utility\n#include "nexus/utility/'+cat+'/'+name+'.h"\n'
    if cpp!=nc:
     with open(p,'w') as fh: fh.write(nc)
     print('  '+cat+'/'+name+'.cpp'); c+=1
print('Simplified '+str(c))
