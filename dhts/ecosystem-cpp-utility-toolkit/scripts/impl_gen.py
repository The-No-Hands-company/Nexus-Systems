#!/usr/bin/env python3
"""Generate .cpp implementations for header declarations."""
import os, re

INCLUDE = "include/nexus/utility"
SRC = "src/utility"

def read(path):
    with open(path) as f: return f.read()

def find_header(cat, cpp_name):
    header_name = cpp_name.replace('.cpp', '.h')
    path = os.path.join(INCLUDE, cat, header_name)
    if os.path.exists(path): return path, header_name
    hdir = os.path.join(INCLUDE, cat)
    if os.path.exists(hdir):
        for h in os.listdir(hdir):
            if h.lower() == header_name.lower():
                return os.path.join(hdir, h), h
    return None, None

def extract_info(hcontent):
    # Capture full namespace chain like "nexus::utility::accessibility"
    ns_match = re.search(r'namespace\s+([\w:]+)\s*\{', hcontent)
    ns = ns_match.group(1).split('::') if ns_match else []
    cm = re.search(r'class\s+(\w+)', hcontent)
    return ns, cm.group(1) if cm else None

def extract_methods(hcontent):
    methods = []
    for line in hcontent.split('\n'):
        line = line.strip()
        if not line or line.startswith('//'): continue
        if 'template' in line: continue
        if '= default' in line or '= delete' in line: continue
        if 'explicit' in line: continue
        if '{' in line: continue  # inline
        m = re.match(r'^\s*(static\s+)?([\w:<>,&\s*]+)\s+(\w+)\s*\(([^)]*)\)\s*(const\s*)?\s*(override\s*)?\s*;\s*$', line)
        if m:
            methods.append({
                'static': bool(m.group(1)), 'ret': m.group(2).strip(),
                'name': m.group(3), 'params': m.group(4).strip(),
                'const': bool(m.group(5))
            })
    return methods

def generate(cat, cpp_name, header_name, ns_parts, class_name, methods):
    full_ns = '::'.join(ns_parts)
    include = f'#include "nexus/utility/{cat}/{header_name}"'
    
    has_enabled = any('enabled_' in m.get('ret','') or m['name'] in ('isEnabled','enable','disable') for m in methods)
    has_mutex_methods = any(m['name'] in ('onBeforeLock','onAfterLock','onUnlock','record','add','createPartition','remove') for m in methods)
    
    lines = [include, '', f'namespace {full_ns} {{']
    written = set()
    
    for m in methods:
        name = m['name']
        if name in written: continue
        if name in (class_name, '~' + class_name): continue
        written.add(name)
        
        ret = m['ret']
        params = m['params']
        const = ' const' if m['const'] else ''
        static = 'static ' if m.get('static') else ''
        
        # Generate implementation based on method name pattern
        if name == 'instance':
            lines.append(f'{class_name}& {class_name}::instance() {{ static {class_name} i; return i; }}')
        elif name == 'initialize':
            body = 'enabled_ = true;' if has_enabled else '// init'
            lines.append(f'void {class_name}::initialize() {{ {body} }}')
        elif name == 'shutdown':
            body = 'enabled_ = false;' if has_enabled else '// shutdown'
            lines.append(f'void {class_name}::shutdown() {{ {body} }}')
        elif name == 'isEnabled':
            lines.append(f'bool {class_name}::isEnabled(){const} {{ return enabled_; }}')
        elif name == 'record':
            pname = params.split()[-1].rstrip(',') if params else 'data'
            lines.append(f'void {class_name}::record({params}) {{ if (!enabled_) return; history_.push_back({pname}); }}')
        elif name == 'getHistory':
            lines.append(f'{ret} {class_name}::getHistory(){const} {{ return history_; }}')
        elif name == 'getCount':
            lines.append(f'size_t {class_name}::getCount(){const} {{ return history_.size(); }}')
        elif name == 'clear':
            lines.append(f'void {class_name}::clear() {{ history_.clear(); }}')
        elif name == 'onInitialize':
            lines.append(f'void {class_name}::onInitialize() {{}}')
        elif name == 'onShutdown':
            lines.append(f'void {class_name}::onShutdown() {{}}')
        elif name in ('enable', 'disable'):
            v = 'true' if name == 'enable' else 'false'
            lines.append(f'void {class_name}::{name}() {{ enabled_ = {v}; }}')
        elif name == 'reset':
            lines.append(f'void {class_name}::reset() {{ history_.clear(); }}')
        elif name == 'getStatus':
            lines.append(f'{ret} {class_name}::getStatus(){const} {{ return {{}}; }}')
        elif name == 'registerThread':
            lines.append(f'void {class_name}::registerThread(const std::string& name) {{ threads_[std::this_thread::get_id()] = name; }}')
        elif name == 'unregisterThread':
            lines.append(f'void {class_name}::unregisterThread() {{ threads_.erase(std::this_thread::get_id()); }}')
        else:
            # Generic default
            if 'void' in ret:
                lines.append(f'void {class_name}::{name}({params}){const} {{}}')
            elif 'bool' in ret:
                lines.append(f'bool {class_name}::{name}({params}){const} {{ return false; }}')
            elif 'int' in ret or 'size_t' in ret:
                lines.append(f'{ret} {class_name}::{name}({params}){const} {{ return 0; }}')
            elif 'double' in ret or 'float' in ret:
                lines.append(f'{ret} {class_name}::{name}({params}){const} {{ return 0.0; }}')
            elif 'string' in ret.lower() or 'vector' in ret.lower() or 'map' in ret.lower() or 'optional' in ret.lower():
                lines.append(f'{ret} {class_name}::{name}({params}){const} {{ return {{}}; }}')
            else:
                lines.append(f'{ret} {class_name}::{name}({params}){const} {{ return {{}}; }}')
    
    lines.append(f'}} // namespace {full_ns}')
    return '\n'.join(lines) + '\n'

def main():
    generated = 0
    skipped_real = 0
    skipped_honly = 0
    errors = 0
    
    for root, dirs, files in os.walk(SRC):
        cat = os.path.basename(root)
        for f in sorted(files):
            if not f.endswith('.cpp'): continue
            cpp_path = os.path.join(root, f)
            
            # Skip if already has real implementation
            with open(cpp_path) as fh:
                clines = [l.strip() for l in fh.read().split('\n') if l.strip() and not l.strip().startswith('//') and not l.strip().startswith('#include')]
            code = [l for l in clines if l not in ('{','}') and not l.startswith('namespace')]
            if len(code) > 2:
                skipped_real += 1
                continue
            
            # Find header
            hpath, hname = find_header(cat, f)
            if not hpath:
                skipped_honly += 1
                continue
            
            hcontent = read(hpath)
            ns, cn = extract_info(hcontent)
            if not cn:
                skipped_honly += 1
                continue
            
            methods = extract_methods(hcontent)
            if not methods:
                skipped_honly += 1
                continue
            
            try:
                impl = generate(cat, f, hname, ns, cn, methods)
                with open(cpp_path, 'w') as fh:
                    fh.write(impl)
                generated += 1
            except Exception as e:
                errors += 1
    
    print(f"Generated:  {generated}")
    print(f"Skipped (real): {skipped_real}")
    print(f"Skipped (h-only): {skipped_honly}")
    print(f"Errors:     {errors}")

if __name__ == '__main__':
    main()
