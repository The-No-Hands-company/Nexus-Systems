#!/usr/bin/env python3
"""Read each header, find actual members, generate matching .cpp"""
import os, re

INCLUDE = "include/nexus/utility"
SRC = "src/utility"

def read(p):
    try:
        with open(p) as f: return f.read()
    except: return ""

def find_header(cat, cpp_name):
    hname = cpp_name.replace('.cpp', '.h')
    path = os.path.join(INCLUDE, cat, hname)
    if os.path.exists(path): return path, hname
    hdir = os.path.join(INCLUDE, cat)
    if os.path.exists(hdir):
        for h in os.listdir(hdir):
            if h.lower() == hname.lower():
                return os.path.join(hdir, h), h
    return None, None

def members_exist(h):
    """Find what member variables actually exist in the header"""
    return {
        'enabled_': 'bool enabled_' in h or 'enabled_ =' in h or 'std::atomic<bool> enabled_' in h,
        'history_': 'history_' in h,
        'mutex_': 'mutex mutex_' in h or 'shared_mutex mutex_' in h or 'std::mutex' in h,
    }

def extract_methods(h):
    methods = []
    for line in h.split('\n'):
        line = line.strip()
        if not line or line.startswith('//'): continue
        if 'template' in line or '= default' in line or '= delete' in line: continue
        if 'explicit' in line or 'typedef' in line or 'using ' in line: continue
        if 'operator' in line: continue
        if '{' in line: continue
        m = re.match(r'^\s*(static\s+)?(virtual\s+)?([\w:<>,&\s*]+)\s+(\w+)\s*\(([^)]*)\)\s*(const\s*)?\s*(override\s*)?\s*;\s*$', line)
        if m:
            methods.append({
                'ret': m.group(3).strip(),
                'name': m.group(4),
                'params': m.group(5).strip(),
                'const': bool(m.group(6)),
            })
    return methods

def gen_method(cn, m, mem):
    name, ret, params, const = m['name'], m['ret'], m['params'], m['const']
    c = ' const' if const else ''
    
    # Instance pattern
    if name == 'instance':
        return f'{cn}& {cn}::instance() {{ static {cn} i; return i; }}'
    
    # Init/shutdown
    if name == 'initialize':
        return f'void {cn}::initialize({params}){{{c} {"enabled_ = true;" if mem["enabled_"] else ""}}}'
    if name == 'shutdown':
        return f'void {cn}::shutdown(){{{c} {"enabled_ = false;" if mem["enabled_"] else ""}}}'
    if name == 'isEnabled':
        if mem['enabled_']:
            return f'bool {cn}::isEnabled(){c} {{ return enabled_; }}'
        return f'bool {cn}::isEnabled(){c} {{ return false; }}'
    
    # History/recording pattern
    if name == 'record' or (name == 'add' and 'string' in params.lower()):
        pn = params.split()[-1].rstrip(',') if params and params.split() else 'e'
        guard = 'if (!enabled_) return; ' if mem['enabled_'] else ''
        hist = f'history_.push_back({pn});' if mem['history_'] else ''
        return f'void {cn}::{name}({params}){{{c} {guard}{hist}}}'
    if name == 'getHistory':
        if mem['history_']: return f'{ret} {cn}::getHistory(){c} {{ return history_; }}'
        return f'{ret} {cn}::getHistory(){c} {{ return {{}}; }}'
    if name == 'getCount':
        if mem['history_']: return f'size_t {cn}::getCount(){c} {{ return history_.size(); }}'
        return f'size_t {cn}::getCount(){c} {{ return 0; }}'
    if name == 'clear':
        clr = 'history_.clear();' if mem['history_'] else ''
        return f'void {cn}::clear(){{ {clr} }}'
    if name == 'reset':
        clr = 'history_.clear();' if mem['history_'] else ''
        return f'void {cn}::reset(){{ {clr} }}'
    
    # Enable/disable
    if name == 'enable':
        v = 'enabled_ = true;' if mem['enabled_'] else ''
        return f'void {cn}::enable(){{ {v} }}'
    if name == 'disable':
        v = 'enabled_ = false;' if mem['enabled_'] else ''
        return f'void {cn}::disable(){{ {v} }}'
    
    # Generic
    if 'void' == ret:
        return f'void {cn}::{name}({params}){{{c} }}'
    if 'bool' == ret:
        return f'bool {cn}::{name}({params}){{{c} return false; }}'
    if ret in ('int','size_t','uint32_t','uint64_t','int64_t'):
        return f'{ret} {cn}::{name}({params}){{{c} return 0; }}'
    if ret in ('double','float'):
        return f'{ret} {cn}::{name}({params}){{{c} return 0.0; }}'
    return f'{ret} {cn}::{name}({params}){{{c} return {{}}; }}'

def main():
    done = 0
    for root, dirs, files in os.walk(SRC):
        cat = os.path.basename(root)
        for f in sorted(files):
            if not f.endswith('.cpp'): continue
            cpp_path = os.path.join(root, f)
            
            # Check if it's a stub
            with open(cpp_path) as fh:
                lines = [l.strip() for l in fh.read().split('\n') if l.strip() and not l.strip().startswith('//') and not l.strip().startswith('#include')]
            code = [l for l in lines if l not in ('{','}') and not l.startswith('namespace')]
            if len(code) > 1: continue  # Already has implementation
            
            hpath, hname = find_header(cat, f)
            if not hpath: continue
            
            h = read(hpath)
            if not h: continue
            
            ns_m = re.search(r'namespace\s+([\w:]+)\s*\{', h)
            ns = ns_m.group(1).split('::') if ns_m else []
            cn_m = re.search(r'class\s+(\w+)', h)
            if not cn_m: continue
            cn = cn_m.group(1)
            
            mem = members_exist(h)
            methods = extract_methods(h)
            if not methods: continue
            
            include = f'#include "nexus/utility/{cat}/{hname}"'
            full_ns = '::'.join(ns)
            
            lines = [include, '', f'namespace {full_ns} {{']
            written = set()
            for m in methods:
                if m['name'] in written: continue
                if m['name'] in (cn, '~' + cn): continue
                written.add(m['name'])
                lines.append('    ' + gen_method(cn, m, mem))
            lines.append(f'}} // namespace {full_ns}')
            lines.append('')
            
            impl = '\n'.join(lines) + '\n'
            with open(cpp_path, 'w') as fh:
                fh.write(impl)
            done += 1
    
    print(f'Updated: {done}')

if __name__ == '__main__':
    main()
