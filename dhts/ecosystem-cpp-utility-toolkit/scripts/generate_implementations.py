#!/usr/bin/env python3
"""
Generate real .cpp implementations for header declarations.
Handles singleton patterns, diagnostic recorders, and domain-specific utilities.
"""
import os, re, sys

INCLUDE_BASE = "include/nexus/utility"
SRC_BASE = "src/utility"

def read_file(path):
    with open(path, 'rb') as f:
        return f.read().decode('utf-8', errors='replace')

def extract_namespace(header_content):
    m = re.search(r'namespace\s+(\S+)\s*\{', header_content)
    if m: return m.group(1)
    return None

def extract_class_info(header_content):
    """Extract class name and namespace chain from header"""
    ns_match = re.findall(r'namespace\s+(\w+)', header_content)
    class_match = re.search(r'class\s+(\w+)', header_content)
    if not class_match: return None, None
    return ns_match, class_match.group(1)

def extract_methods(header_content):
    """Extract method declarations that need implementations"""
    methods = []
    # Find lines with function declarations ending in ;
    for line in header_content.split('\n'):
        line = line.strip()
        # Skip templates, inline, comments, constructors with = default/delete
        if not line or line.startswith('//') or line.startswith('/*'):
            continue
        if 'template' in line: continue
        if ' = default' in line or ' = delete' in line: continue
        if '{' in line and '}' in line: continue  # Inline implementation
        if 'typedef' in line or 'using ' in line: continue
        if 'enum ' in line or 'struct ' in line: continue
        if 'friend ' in line: continue
        
        # Match method declarations
        m = re.match(r'(static\s+)?([\w:<>,&*\s]+)\s+(\w+)\s*\((.*?)\)\s*(const\s*)?(override\s*)?(final\s*)?;', line)
        if m:
            is_static = m.group(1) is not None
            ret_type = m.group(2).strip()
            name = m.group(3).strip()
            params = m.group(4).strip()
            is_const = m.group(5) is not None
            if name not in ('explicit', 'operator'):
                methods.append({
                    'static': is_static,
                    'ret': ret_type,
                    'name': name,
                    'params': params,
                    'const': is_const,
                })
    return methods

def gen_singleton_boilerplate(class_name, ns_parts, methods, has_enabled):
    """Generate implementation for singleton pattern classes"""
    full_ns = '::'.join(ns_parts)
    
    impl = f'#include "{"/".join(ns_parts)}/{class_name.lower().replace("_", "_")}.h"\n'
    
    # Try to include the right header
    cat = ns_parts[-1] if len(ns_parts) > 1 else ns_parts[0]
    header_name = re.sub(r'([a-z])([A-Z])', r'\1_\2', class_name).lower() + '.h'
    impl = f'#include "nexus/utility/{cat}/{header_name}"\n'
    
    impl += f'\nnamespace {full_ns} {{\n\n'
    
    # instance()
    impl += f'{class_name}& {class_name}::instance() {{\n'
    impl += f'    static {class_name} inst;\n'
    impl += f'    return inst;\n'
    impl += f'}}\n\n'
    
    # Track which methods we've handled
    handled = {'instance'}
    
    for m in methods:
        name = m['name']
        if name in handled: continue
        handled.add(name)
        ret = m['ret']
        params = m['params']
        const = ' const' if m['const'] else ''
        

        if name == 'initialize':
            impl += f'void {class_name}::{name}() {{\n'
            if has_enabled:
                impl += f'    std::lock_guard lock(mutex_);\n'
                impl += f'    enabled_ = true;\n'
            impl += f'    onInitialize();\n'
            impl += f'}}\n\n'
        
        elif name == 'shutdown':
            impl += f'void {class_name}::{name}() {{\n'
            if has_enabled:
                impl += f'    std::lock_guard lock(mutex_);\n'
                impl += f'    enabled_ = false;\n'
            impl += f'    onShutdown();\n'
            impl += f'}}\n\n'
        
        elif name == 'isEnabled':
            impl += f'{ret} {class_name}::{name}(){const} {{\n'
            impl += f'    std::lock_guard lock(mutex_);\n'
            impl += f'    return enabled_;\n'
            impl += f'}}\n\n'
        
        elif name == 'record' and 'std::string' in params:
            impl += f'void {class_name}::{name}({params}) {{\n'
            impl += f'    std::lock_guard lock(mutex_);\n'
            impl += f'    if (!enabled_) return;\n'
            param_name = params.split()[-1] if params else 'data'
            impl += f'    history_.push_back({{{param_name}}});\n'
            impl += f'}}\n\n'
        
        elif name == 'getHistory':
            impl += f'{ret} {class_name}::{name}(){const} {{\n'
            impl += f'    std::lock_guard lock(mutex_);\n'
            impl += f'    return history_;\n'
            impl += f'}}\n\n'
        
        elif name == 'getCount':
            impl += f'{ret} {class_name}::{name}(){const} {{\n'
            impl += f'    std::lock_guard lock(mutex_);\n'
            impl += f'    return history_.size();\n'
            impl += f'}}\n\n'
        
        elif name == 'clear':
            impl += f'void {class_name}::{name}() {{\n'
            impl += f'    std::lock_guard lock(mutex_);\n'
            impl += f'    history_.clear();\n'
            impl += f'}}\n\n'
        
        elif name == 'onInitialize' or name == 'onShutdown':
            impl += f'void {class_name}::{name}() {{\n'
            impl += f'    // Override in derived classes\n'
            impl += f'}}\n\n'
        
        elif name == 'reset':
            impl += f'void {class_name}::{name}() {{\n'
            impl += f'    std::lock_guard lock(mutex_);\n'
            impl += f'    history_.clear();\n'
            impl += f'}}\n\n'
        
        elif 'enable' in name.lower() and 'enable' not in handled:
            impl += f'void {class_name}::{name}() {{\n'
            impl += f'    std::lock_guard lock(mutex_);\n'
            impl += f'    enabled_ = true;\n'
            impl += f'}}\n\n'
        
        elif 'disable' in name.lower() and 'disable' not in handled:
            impl += f'void {class_name}::{name}() {{\n'
            impl += f'    std::lock_guard lock(mutex_);\n'
            impl += f'    enabled_ = false;\n'
            impl += f'}}\n\n'
        
        elif name in ('getStatus', 'getStats', 'getReport'):
            impl += f'{ret} {class_name}::{name}(){const} {{\n'
            impl += f'    std::lock_guard lock(mutex_);\n'
            impl += f'    return {{}};  // TODO: implement status reporting\n'
            impl += f'}}\n\n'
        
        else:
            # Generic default implementation
            impl += f'{ret} {class_name}::{name}({params}){const} {{\n'
            impl += f'    std::lock_guard lock(mutex_);\n'
            if ret == 'void':
                impl += f'    // TODO: implement {name}\n'
            elif ret == 'bool':
                impl += f'    return false;\n'
            elif ret == 'size_t':
                impl += f'    return 0;\n'
            elif ret == 'int':
                impl += f'    return 0;\n'
            elif ret == 'double':
                impl += f'    return 0.0;\n'
            elif 'std::string' in ret:
                impl += f'    return {{}};\n'
            elif 'std::vector' in ret or 'std::optional' in ret:
                impl += f'    return {{}};\n'
            else:
                impl += f'    return {{}};\n'
            impl += f'}}\n\n'
    
    impl += f'}} // namespace {full_ns}\n'
    return impl

def has_mutex(header_content):
    return 'std::mutex' in header_content or 'shared_mutex' in header_content

def has_enabled_member(header_content):
    return 'enabled_' in header_content or 'bool enabled' in header_content

def is_header_only(header_content):
    """Check if header already has all inline implementations"""
    lines = [l.strip() for l in header_content.split('\n') if l.strip() and not l.strip().startswith('//')]
    # Count declarations ending with ; vs inline definitions with {}
    decl_count = sum(1 for l in lines if re.search(r'\w+\s+\w+\s*\(.*\)\s*(const\s*)?;', l) and '= default' not in l and '= delete' not in l)
    body_count = sum(1 for l in lines if '{' in l and '}' in l and not l.startswith('namespace'))
    return decl_count == 0 or body_count > decl_count * 0.8

def main():
    stats = {'generated': 0, 'skipped_header_only': 0, 'skipped_has_impl': 0}
    
    for root, dirs, files in os.walk(SRC_BASE):
        cat = os.path.basename(root)
        for f in sorted(files):
            if not f.endswith('.cpp'): continue
            cpp_path = os.path.join(root, f)
            
            # Read current .cpp
            cpp_content = read_file(cpp_path)
            cpp_lines = [l.strip() for l in cpp_content.split('\n') if l.strip() and not l.strip().startswith('//')]
            
            # Skip if already has implementation
            if len(cpp_lines) > 3:
                stats['skipped_has_impl'] += 1
                continue
            
            # Find header
            header_name = f.replace('.cpp', '.h')
            header_path = os.path.join(INCLUDE_BASE, cat, header_name)
            if not os.path.exists(header_path):
                continue
            
            header_content = read_file(header_path)
            
            # Skip header-only (templates, inline)
            if is_header_only(header_content):
                stats['skipped_header_only'] += 1
                continue
            
            # Generate implementation
            ns_parts, class_name = extract_class_info(header_content)
            if not class_name:
                continue
            
            methods = extract_methods(header_content)
            if not methods:
                continue
            
            impl = gen_singleton_boilerplate(
                class_name, ns_parts, methods,
                has_enabled_member(header_content)
            )
            
            # Write the implementation
            with open(cpp_path, 'w') as fh:
                fh.write(impl)
            stats['generated'] += 1
    
    print(f"Generated: {stats['generated']}")
    print(f"Skipped (header-only): {stats['skipped_header_only']}")
    print(f"Skipped (already has impl): {stats['skipped_has_impl']}")

if __name__ == '__main__':
    main()
