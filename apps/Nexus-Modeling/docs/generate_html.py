#!/usr/bin/env python3
"""
HTML Documentation Generator for Nexus Modeling
Converts Markdown docs to a navigable HTML site with dark/light theme toggle.
"""

import os
import re
import shutil
from pathlib import Path
from typing import Dict, List, Optional

DOCS_ROOT = Path(__file__).parent
HTML_ROOT = DOCS_ROOT / "html"

# Theme toggle button HTML
THEME_TOGGLE_BTN = '''
<button class="btn btn-icon" id="theme-toggle" aria-label="Toggle dark/light theme" title="Toggle theme (Ctrl+Shift+L)">
    <svg class="theme-icon-sun" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="display: none;">
        <circle cx="12" cy="12" r="5"></circle>
        <line x1="12" y1="1" x2="12" y2="3"></line>
        <line x1="12" y1="21" x2="12" y2="23"></line>
        <line x1="4.22" y1="4.22" x2="5.64" y2="5.64"></line>
        <line x1="18.36" y1="18.36" x2="19.78" y2="19.78"></line>
        <line x1="1" y1="12" x2="3" y2="12"></line>
        <line x1="21" y1="12" x2="23" y2="12"></line>
        <line x1="4.22" y1="19.78" x2="5.64" y2="18.36"></line>
        <line x1="18.36" y1="5.64" x2="19.78" y2="4.22"></line>
    </svg>
    <svg class="theme-icon-moon" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="display: none;">
        <path d="M21 12.79A9 9 0 1 1 11.21 3 7 7 0 0 0 21 12.79z"></path>
    </svg>
</button>
'''

# JavaScript for theme handling (shared across all pages)
THEME_JS = """
<script>
// Theme management
const THEME_KEY = 'nexus-docs-theme';
const themeToggle = document.getElementById('theme-toggle');
const sunIcon = themeToggle.querySelector('.theme-icon-sun');
const moonIcon = themeToggle.querySelector('.theme-icon-moon');

function applyTheme(theme) {
    document.documentElement.setAttribute('data-theme', theme);
    if (theme === 'light') {
        sunIcon.style.display = 'block';
        moonIcon.style.display = 'none';
    } else {
        sunIcon.style.display = 'none';
        moonIcon.style.display = 'block';
    }
    localStorage.setItem(THEME_KEY, theme);
}

function getPreferredTheme() {
    const stored = localStorage.getItem(THEME_KEY);
    if (stored) return stored;
    return window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light';
}

function initTheme() {
    const theme = getPreferredTheme();
    applyTheme(theme);
}

function toggleTheme() {
    const current = document.documentElement.getAttribute('data-theme') || 'dark';
    applyTheme(current === 'dark' ? 'light' : 'dark');
}

themeToggle.addEventListener('click', toggleTheme);

// Keyboard shortcut: Ctrl+Shift+L
document.addEventListener('keydown', (e) => {
    if (e.ctrlKey && e.shiftKey && e.key === 'L') {
        e.preventDefault();
        toggleTheme();
    }
});

// Listen for system theme changes
window.matchMedia('(prefers-color-scheme: dark)').addEventListener('change', (e) => {
    if (!localStorage.getItem(THEME_KEY)) {
        applyTheme(e.matches ? 'dark' : 'light');
    }
});

// Initialize theme immediately to prevent flash
initTheme();

// Mobile sidebar toggle
const sidebar = document.getElementById('sidebar');
const mobileMenuBtn = document.getElementById('mobile-menu-btn');

if (mobileMenuBtn) {
    mobileMenuBtn.addEventListener('click', () => {
        sidebar.classList.toggle('open');
    });
}

document.addEventListener('click', (e) => {
    if (window.innerWidth <= 768) {
        if (!sidebar.contains(e.target) && !e.target.closest('.header')) {
            sidebar.classList.remove('open');
        }
    }
});

// Smooth scroll for anchor links
document.querySelectorAll('a[href^="#"]').forEach(anchor => {
    anchor.addEventListener('click', function(e) {
        const target = document.querySelector(this.getAttribute('href'));
        if (target) {
            e.preventDefault();
            target.scrollIntoView({ behavior: 'smooth', block: 'start' });
            if (window.innerWidth <= 768) {
                sidebar.classList.remove('open');
            }
        }
    });
});

// Active link highlighting based on scroll position
const sections = document.querySelectorAll('.content h2, .content h3');
const sidebarLinks = document.querySelectorAll('.sidebar-link[href^="#"]');

function updateActiveLink() {
    let current = '';
    sections.forEach(section => {
        const rect = section.getBoundingClientRect();
        if (rect.top <= 100) {
            current = '#' + section.id;
        }
    });

    sidebarLinks.forEach(link => {
        link.classList.remove('active');
        if (link.getAttribute('href') === current) {
            link.classList.add('active');
        }
    });
}

window.addEventListener('scroll', updateActiveLink, { passive: true });
window.addEventListener('load', updateActiveLink);

// Highlight.js initialization
if (typeof hljs !== 'undefined') {
    hljs.highlightAll();
}

// Mobile menu button visibility
const mobileMenuBtn = document.getElementById('mobile-menu-btn');
function checkMobile() {
    if (window.innerWidth <= 768) {
        mobileMenuBtn.style.display = 'flex';
    } else {
        mobileMenuBtn.style.display = 'none';
        document.getElementById('sidebar').classList.remove('open');
    }
}
checkMobile();
window.addEventListener('resize', checkMobile);

mobileMenuBtn.addEventListener('click', () => {
    document.getElementById('sidebar').classList.toggle('open');
});
</script>
"""

# CSS content
CSS_CONTENT = """
/* Nexus Modeling Documentation Styles - Dark Mode First */
:root {
    --primary: #3b82f6;
    --primary-dark: #2563eb;
    --primary-light: #60a5fa;
    --accent: #06b6d4;
    --accent-light: #22d3ee;
    --secondary: #0f172a;
    --secondary-light: #1e293b;
    --bg: #020617;
    --bg-secondary: #0f172a;
    --bg-tertiary: #1e293b;
    --surface: #1e293b;
    --surface-hover: #334155;
    --text: #f1f5f9;
    --text-secondary: #94a3b8;
    --text-muted: #64748b;
    --border: #334155;
    --border-light: #475569;
    --code-bg: #0f172a;
    --code-text: #e2e8f0;
    --sidebar-width: 300px;
    --header-height: 60px;
    --shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.4);
    --shadow-lg: 0 10px 15px -3px rgba(0, 0, 0, 0.5);
    --radius: 6px;
    --transition: 0.15s ease;
}

/* Light mode variables */
[data-theme="light"] {
    --primary: #2563eb;
    --primary-dark: #1d4ed8;
    --primary-light: #3b82f6;
    --accent: #0891b2;
    --accent-light: #06b6d4;
    --secondary: #f8fafc;
    --secondary-light: #f1f5f9;
    --bg: #ffffff;
    --bg-secondary: #f8fafc;
    --bg-tertiary: #f1f5f9;
    --surface: #ffffff;
    --surface-hover: #f1f5f9;
    --text: #0f172a;
    --text-secondary: #475569;
    --text-muted: #94a3b8;
    --border: #e2e8f0;
    --border-light: #cbd5e1;
    --code-bg: #f1f5f9;
    --code-text: #1e293b;
    --shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1);
    --shadow-lg: 0 10px 15px -3px rgba(0, 0, 0, 0.1);
}

* { box-sizing: border-box; margin: 0; padding: 0; }

html {
    font-size: 16px;
    scroll-behavior: smooth;
}

body {
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif;
    background: var(--bg);
    color: var(--text);
    line-height: 1.7;
    min-height: 100vh;
    display: flex;
    transition: background var(--transition), color var(--transition);
}

a { color: var(--primary); text-decoration: none; transition: color var(--transition); }
a:hover { color: var(--primary-light); text-decoration: underline; }

:focus-visible {
    outline: 2px solid var(--primary);
    outline-offset: 2px;
}

/* Sidebar */
.sidebar {
    width: var(--sidebar-width);
    background: var(--secondary);
    color: var(--text);
    height: 100vh;
    position: fixed;
    left: 0;
    top: 0;
    overflow-y: auto;
    padding: 1.5rem 1rem;
    border-right: 1px solid var(--border);
    transition: transform 0.3s ease, background var(--transition), border-color var(--transition);
    z-index: 100;
}

.sidebar-header {
    padding-bottom: 1.5rem;
    margin-bottom: 1.5rem;
    border-bottom: 1px solid var(--border);
}

.sidebar-title {
    font-size: 1.1rem;
    font-weight: 600;
    color: var(--accent);
    margin-bottom: 0.5rem;
}

.sidebar-subtitle {
    font-size: 0.85rem;
    color: var(--text-muted);
}

.sidebar-section { margin-bottom: 1.5rem; }

.sidebar-section-title {
    font-size: 0.7rem;
    text-transform: uppercase;
    letter-spacing: 0.08em;
    color: var(--text-muted);
    margin-bottom: 0.5rem;
    padding-left: 0.5rem;
    font-weight: 600;
}

.sidebar-link {
    display: block;
    padding: 0.5rem 0.75rem;
    border-radius: var(--radius);
    color: var(--text-secondary);
    font-size: 0.875rem;
    margin-bottom: 0.125rem;
    transition: all var(--transition);
    border: 1px solid transparent;
}

.sidebar-link:hover {
    background: var(--surface-hover);
    color: var(--text);
    text-decoration: none;
    border-color: var(--border);
}

.sidebar-link.active {
    background: var(--primary);
    color: white;
    border-color: var(--primary);
}

.sidebar-link.active:hover {
    color: white;
}

.sidebar-link.external::after {
    content: " \u2197";
    font-size: 0.7em;
    opacity: 0.6;
}

/* Main Content */
.main-content {
    flex: 1;
    margin-left: var(--sidebar-width);
    min-height: 100vh;
    transition: margin-left 0.3s ease;
}

.header {
    height: var(--header-height);
    background: var(--surface);
    border-bottom: 1px solid var(--border);
    position: sticky;
    top: 0;
    z-index: 100;
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 0 2rem;
    transition: background var(--transition), border-color var(--transition);
}

.header-title {
    font-size: 1.125rem;
    font-weight: 600;
    color: var(--text);
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
    max-width: 500px;
}

.header-actions {
    display: flex;
    gap: 0.75rem;
    align-items: center;
}

.btn {
    padding: 0.5rem 1rem;
    border-radius: var(--radius);
    font-size: 0.875rem;
    font-weight: 500;
    cursor: pointer;
    border: 1px solid transparent;
    transition: all var(--transition);
    display: inline-flex;
    align-items: center;
    gap: 0.5rem;
    text-decoration: none;
}

.btn-primary {
    background: var(--primary);
    color: white;
    border-color: var(--primary);
}

.btn-primary:hover {
    background: var(--primary-dark);
    border-color: var(--primary-dark);
    color: white;
    text-decoration: none;
}

.btn-secondary {
    background: var(--surface);
    color: var(--text);
    border-color: var(--border);
}

.btn-secondary:hover {
    background: var(--surface-hover);
    border-color: var(--border-light);
    color: var(--text);
    text-decoration: none;
}

.btn-icon {
    background: var(--surface);
    color: var(--text);
    border-color: var(--border);
    padding: 0.5rem;
    border-radius: var(--radius);
}

.btn-icon:hover {
    background: var(--surface-hover);
    border-color: var(--primary);
    color: var(--primary);
}

.content {
    max-width: 900px;
    margin: 0 auto;
    padding: 2.5rem 2rem;
}

.content h1 {
    font-size: 2.25rem;
    font-weight: 700;
    color: var(--text);
    margin-bottom: 1rem;
    padding-bottom: 0.75rem;
    border-bottom: 2px solid var(--border);
    line-height: 1.3;
}

.content h2 {
    font-size: 1.5rem;
    font-weight: 600;
    color: var(--text);
    margin-top: 2.5rem;
    margin-bottom: 1rem;
    padding-top: 1rem;
    scroll-margin-top: 80px;
}

.content h3 {
    font-size: 1.25rem;
    font-weight: 600;
    color: var(--text);
    margin-top: 2rem;
    margin-bottom: 0.75rem;
    scroll-margin-top: 80px;
}

.content h4 {
    font-size: 1.1rem;
    font-weight: 600;
    color: var(--text);
    margin-top: 1.5rem;
    margin-bottom: 0.5rem;
    scroll-margin-top: 80px;
}

.content p { margin-bottom: 1rem; color: var(--text-secondary); }

.content ul, .content ol {
    margin-bottom: 1rem;
    padding-left: 1.5rem;
    color: var(--text-secondary);
}

.content li { margin-bottom: 0.5rem; }

.content li::marker { color: var(--text-muted); }

.content code {
    font-family: 'JetBrains Mono', 'Fira Code', 'SF Mono', Monaco, monospace;
    font-size: 0.9em;
    background: var(--code-bg);
    color: var(--accent);
    padding: 0.15rem 0.4rem;
    border-radius: 4px;
    border: 1px solid var(--border);
}

.content pre {
    background: var(--code-bg);
    color: var(--code-text);
    padding: 1.25rem;
    border-radius: 8px;
    overflow-x: auto;
    margin-bottom: 1.5rem;
    font-size: 0.875rem;
    line-height: 1.7;
    border: 1px solid var(--border);
    position: relative;
}

.content pre code {
    background: none;
    padding: 0;
    color: inherit;
    font-size: inherit;
    border: none;
}

.content table {
    width: 100%;
    border-collapse: collapse;
    margin-bottom: 1.5rem;
    font-size: 0.9rem;
    border: 1px solid var(--border);
    border-radius: var(--radius);
    overflow: hidden;
}

.content th, .content td {
    padding: 0.75rem 1rem;
    border: 1px solid var(--border);
    text-align: left;
}

.content th {
    background: var(--surface);
    font-weight: 600;
    color: var(--text);
}

.content tr:nth-child(even) td {
    background: var(--bg-secondary);
}

.content tr:hover td {
    background: var(--surface-hover);
}

.content blockquote {
    border-left: 4px solid var(--primary);
    padding-left: 1.5rem;
    margin: 1.5rem 0;
    color: var(--text-secondary);
    font-style: italic;
    background: var(--bg-secondary);
    padding: 1rem 1.5rem;
    border-radius: 0 var(--radius) var(--radius) 0;
}

.content hr {
    border: none;
    border-top: 1px solid var(--border);
    margin: 2rem 0;
}

.content .toc {
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 1.5rem;
    margin-bottom: 2rem;
    position: sticky;
    top: 80px;
}

.content .toc h3 {
    margin-top: 0;
    font-size: 0.875rem;
    font-weight: 600;
    text-transform: uppercase;
    letter-spacing: 0.05em;
    color: var(--text-muted);
    margin-bottom: 1rem;
    padding-bottom: 0.75rem;
    border-bottom: 1px solid var(--border);
}

.content .toc ul {
    list-style: none;
    padding-left: 0;
}

.content .toc li { margin-bottom: 0.5rem; }

.content .toc a {
    color: var(--text-secondary);
    font-size: 0.9rem;
    transition: color var(--transition);
}

.content .toc a:hover {
    color: var(--primary);
    text-decoration: none;
}

.content .toc ul ul {
    padding-left: 1.5rem;
    margin-top: 0.5rem;
    border-left: 1px solid var(--border);
    margin-left: -0.75rem;
}

.content img {
    max-width: 100%;
    height: auto;
    border-radius: var(--radius);
    border: 1px solid var(--border);
}

/* Syntax highlighting overrides */
.hljs { background: var(--code-bg) !important; color: var(--code-text) !important; }
.hljs-keyword { color: #c678dd; }
.hljs-function { color: #61afef; }
.hljs-string { color: #98c379; }
.hljs-comment { color: #5c6370; }
.hljs-number { color: #d19a66; }
.hljs-type { color: #e5c07b; }
.hljs-title { color: #e06c75; }
.hljs-attribute { color: #d19a66; }
.hljs-selector-tag { color: #e06c75; }
.hljs-selector-class { color: #61afef; }
.hljs-selector-id { color: #c678dd; }
.hljs-regexp { color: #56b6c2; }
.hljs-symbol { color: #56b6c2; }
.hljs-built_in { color: #e06c75; }
.hljs-literal { color: #56b6c2; }

/* Mobile */
@media (max-width: 768px) {
    .sidebar {
        transform: translateX(-100%);
        transition: transform 0.3s ease;
        box-shadow: var(--shadow-lg);
    }
    .sidebar.open { transform: translateX(0); }
    .main-content { margin-left: 0; }
    .header { padding: 0 1rem; }
    .header-title { max-width: 200px; font-size: 1rem; }
    .content { padding: 1.5rem 1rem; }
    .content h1 { font-size: 1.75rem; }
    .content h2 { font-size: 1.35rem; }
    .btn-text { display: none; }
}

/* Scrollbar styling */
::-webkit-scrollbar { width: 8px; height: 8px; }
::-webkit-scrollbar-track { background: var(--bg); }
::-webkit-scrollbar-thumb { background: var(--border); border-radius: 4px; }
::-webkit-scrollbar-thumb:hover { background: var(--border-light); }
::-webkit-scrollbar-corner { background: var(--bg); }

/* Selection */
::selection { background: var(--primary); color: white; }
::-moz-selection { background: var(--primary); color: white; }

/* Print styles */
@media print {
    .sidebar, .header, .toc, .btn { display: none !important; }
    .main-content { margin-left: 0; }
    .content { padding: 0; max-width: none; }
    body { background: white; color: black; }
    .content pre { border: 1px solid #ccc; }
}
"""

def compute_relative_prefix(current_page: str) -> str:
    """Compute the relative path prefix from current_page to HTML root."""
    if not current_page:
        return ""
    depth = current_page.count('/')
    if depth == 0:
        return ""
    return "../" * depth

def markdown_to_html(md_content: str) -> str:
    """Convert markdown to HTML with proper inline formatting within blocks."""
    html = md_content
    
    # 1. Headers with IDs for TOC and anchor links
    def add_header_id(match):
        level = len(match.group(1))
        text = match.group(2)
        anchor = re.sub(r'[^\w]+', '-', text.lower()).strip('-')
        return f'<h{level} id="{anchor}">{text}</h{level}>'
    
    html = re.sub(r'^(#{1,4})\s+(.+)$', add_header_id, html, flags=re.MULTILINE)
    
    # 2. Code blocks (process first, before other formatting)
    def replace_code_block(match):
        lang = match.group(1) or ''
        code = match.group(2).rstrip()
        return f'<pre><code class="language-{lang}">{code}</code></pre>'
    
    html = re.sub(r'```(\w*)\n(.*?)\n```', replace_code_block, html, flags=re.DOTALL)
    
    # 3. Horizontal rule
    html = re.sub(r'^---$', '<hr>', html, flags=re.MULTILINE)
    
    # 4. Blockquotes
    html = re.sub(r'^>\s*(.+)$', r'<blockquote>\1</blockquote>', html, flags=re.MULTILINE)
    
    # 5. Build block structure (headers, code blocks, lists, paragraphs)
    lines = html.split('\n')
    result = []
    in_para = False
    in_list = False
    list_type = None
    
    for line in lines:
        stripped = line.strip()
        
        list_match = re.match(r'^\s*[-*+]\s+(.+)$', stripped)
        num_match = re.match(r'^\s*\d+\.\s+(.+)$', stripped)
        
        if list_match or num_match:
            if not in_list:
                if in_para:
                    result.append('</p>')
                    in_para = False
                list_type = 'ol' if num_match else 'ul'
                result.append(f'<{list_type}>')
                in_list = True
            content = list_match.group(1) if list_match else num_match.group(1)
            # Process inline formatting in list item content
            content = process_inline_formatting(content)
            result.append(f'<li>{content}</li>')
            in_para = False
            continue
        elif in_list and not stripped:
            result.append(f'</{list_type}>')
            in_list = False
        
        is_block = any(stripped.startswith(tag) for tag in ('<h', '<pre', '<table', '<hr', '<blockquote', '<div', '<ul', '<ol'))
        
        if is_block:
            if in_para:
                result.append('</p>')
                in_para = False
            result.append(line)
            continue
        
        if in_list and stripped and not (re.match(r'^\s*[-*+]\s+', stripped) or re.match(r'^\s*\d+\.\s+', stripped)):
            result.append(f'</{list_type}>')
            in_list = False
        
        if stripped:
            if not in_para:
                result.append('<p>')
                in_para = True
            result.append(line)
        else:
            if in_para:
                result.append('</p>')
                in_para = False
            result.append(line)
    
    if in_para:
        result.append('</p>')
    if in_list:
        result.append(f'</{list_type}>')
    
    html = '\n'.join(result)
    
    # 6. Process inline formatting in text content (outside HTML tags)
    html = process_inline_formatting_in_text(html)
    
    # Fix nested tags
    html = re.sub(r'(</ul>|</ol>)\s*(<h[1-6]>)', r'\1\2', html)
    html = re.sub(r'(</ul>|</ol>)\s*(<pre>)', r'\1\2', html)
    html = re.sub(r'(</ul>|</ol>)\s*(<table>)', r'\1\2', html)
    
    return html


def process_inline_formatting_in_text(html: str) -> str:
    """Process inline formatting (bold, italic, code) in text content only, not inside HTML tags."""
    # Split by HTML tags, process text nodes
    parts = re.split(r'(<[^>]+>)', html)
    result = []
    for part in parts:
        if part.startswith('<') and part.endswith('>'):
            # HTML tag, leave as-is
            result.append(part)
        else:
            # Text content, process inline formatting
            result.append(process_inline_formatting(part))
    return ''.join(result)


def process_inline_formatting(text: str) -> str:
    """Process inline formatting (bold, italic, code) in plain text."""
    # Inline code (process first to avoid conflicts)
    text = re.sub(r'`([^`]+)`', r'<code>\1</code>', text)
    # Bold
    text = re.sub(r'\*\*(.+?)\*\*', r'<strong>\1</strong>', text)
    # Italic (but not bold)
    text = re.sub(r'(?<!\*)\*(?!\*)(.+?)(?<!\*)\*(?!\*)', r'<em>\1</em>', text)
    # Links
    text = re.sub(r'\[(.+?)\]\((.+?)\)', r'<a href="\2">\1</a>', text)
    return text

def extract_toc(html: str) -> List[tuple]:
    """Extract table of contents from HTML."""
    toc = []
    for match in re.finditer(r'<h([1-4]) id="([^"]+)">(.+?)</h\1>', html):
        level = int(match.group(1))
        anchor = match.group(2)
        text = match.group(3)
        toc.append((level, text, anchor))
    return toc

def build_sidebar_nav(current_page: str = "") -> str:
    """Build the sidebar navigation HTML."""
    nav = []
    rel_prefix = compute_relative_prefix(current_page)
    
    sections = [
        ("\U0001F4DA Developer Documentation", "developer", [
            ("Architecture Overview", "architecture.html"),
            ("Kernel API Reference", "kernel-api.html"),
            ("Geometry Kernel", "geometry-kernel.html"),
            ("Render System", "renderer.html"),
            ("Editor Architecture", "editor-architecture.html"),
            ("Build System", "build-system.html"),
            ("Testing", "testing.html"),
            ("Contributing", "contributing.html"),
            ("Adding Geometry Op", "adding-geometry-op.html"),
            ("Adding CAD Feature", "adding-cad-feature.html"),
            ("Mesh Operations", "mesh-operations.html"),
            ("Boolean/CSG Deep Dive", "boolean-csg.html"),
            ("Half-edge Mesh", "halfedge-mesh.html"),
            ("Analytic B-rep", "analytic-brep.html"),
            ("Mesh I/O", "mesh-io.html"),
            ("Tolerance System", "tolerance.html"),
            ("Porting Guide", "porting.html"),
        ]),
        ("\U0001F3A8 Expert User Documentation", "expert", [
            # Expert guides will be added as files are created
        ]),
        ("\U0001F464 Beginner / User Documentation", "beginner", [
            ("Getting Started", "getting-started.html"),
        ]),
        ("\U0001F527 API Reference", "api", [
            # API reference will be added as files are created
        ]),
        ("\U0001F4DA Additional Resources", "resources", [
            ("Design Decisions", "design-decisions.html"),
            ("Testing Strategy", "testing-strategy.html"),
            ("Graphics Kernel", "graphics-kernel.html"),
            ("Vision & Roadmap", "vision-and-roadmap.html"),
            ("Reference Roadmap", "reference-roadmap.html"),
            ("Modeling Industry Parity", "modeling-industry-parity.html"),
            ("Modeling Parity Index", "modeling-parity-index.html"),
            ("Modeling Reference Roadmap", "modeling-reference-roadmap.html"),
        ]),
    ]
    
    for title, _, items in sections:
        nav.append('<div class="sidebar-section">')
        nav.append(f'<div class="sidebar-section-title">{title}</div>')
        for label, href in items:
            active_class = ' active' if href == current_page else ''
            nav.append(f'<a class="sidebar-link{active_class}" href="{rel_prefix}{href}">{label}</a>')
        nav.append('</div>')
    
    return '\n'.join(nav)

def generate_html_page(title: str, content_html: str, current_page: str = "") -> str:
    """Generate a complete HTML page."""
    sidebar = build_sidebar_nav(current_page)
    toc = extract_toc(content_html)
    rel_prefix = compute_relative_prefix(current_page)
    
    toc_html = ''
    if toc:
        toc_html = '<div class="toc"><h3>On This Page</h3><ul>'
        for level, text, anchor in toc:
            indent = '  ' * (level - 1)
            toc_html += f'{indent}<li><a href="#{anchor}">{text}</a></li>\n'
        toc_html += '</ul></div>'
    
    return f'''<!DOCTYPE html>
<html lang="en" data-theme="dark">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>{title} \u2014 Nexus Modeling Docs</title>
    <link rel="stylesheet" href="{rel_prefix}styles.css">
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
    <link href="https://fonts.googleapis.com/css2?family=JetBrains+Mono:wght@400;500&display=swap" rel="stylesheet">
    <script src="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/highlight.min.js"></script>
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/styles/github-dark.min.css">
</head>
<body>
    <nav class="sidebar" id="sidebar">
        <div class="sidebar-header">
            <div class="sidebar-title">Nexus Modeling</div>
            <div class="sidebar-subtitle">Documentation v0.1.0-dev</div>
        </div>
        {sidebar}
    </nav>
    
    <div class="main-content">
        <header class="header">
            <button class="btn btn-icon" id="mobile-menu-btn" aria-label="Toggle menu" style="display: none;">
                <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
                    <line x1="3" y1="12" x2="21" y2="12"></line>
                    <line x1="3" y1="6" x2="21" y2="6"></line>
                    <line x1="3" y1="18" x2="21" y2="18"></line>
                </svg>
            </button>
            <div class="header-title">{title}</div>
            <div class="header-actions">
                {THEME_TOGGLE_BTN}
                <a href="https://github.com/nexus-systems/nexus-modeling" class="btn btn-secondary" target="_blank" rel="noopener">
                    <svg width="18" height="18" viewBox="0 0 24 24" fill="currentColor">
                        <path d="M12 0C5.374 0 0 5.373 0 12c0 5.302 3.438 9.8 8.207 11.387.599.111.793-.261.793-.577v-2.234c-3.338.726-4.033-1.416-4.033-1.416-.546-1.387-1.333-1.756-1.333-1.756-1.089-.745.083-.729.083-.729 1.205.084 1.839 1.237 1.839 1.237 1.07 1.834 2.807 1.304 3.492.997.107-.775.418-1.305.762-1.604-2.665-.305-5.467-1.334-5.467-5.931 0-1.311.469-2.381 1.236-3.221-.124-.303-.535-1.524.117-3.176 0 0 1.008-.322 3.301 1.23A11.509 11.509 0 0112 5.803c1.02.005 2.047.138 3.006.404 2.291-1.552 3.297-1.23 3.297-1.23.653 1.653.242 2.874.118 3.176.77.84 1.235 1.911 1.235 3.221 0 4.609-2.807 5.624-5.479 5.921.43.372.823 1.102.823 2.222v3.293c0 .319.192.694.801.576C20.566 21.797 24 17.3 24 12c0-6.627-5.373-12-12-12z"/>
                    </svg>
                    <span class="btn-text">GitHub</span>
                </a>
                <a href="{rel_prefix}index.html" class="btn btn-primary">
                    <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
                        <path d="M3 9l9-7 9 7v11a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"></path>
                        <polyline points="9 22 9 12 15 12 15 22"></polyline>
                    </svg>
                    <span class="btn-text">Home</span>
                </a>
            </div>
        </header>
        
        <main class="content">
            {toc_html}
            {content_html}
        </main>
    </div>
    
    <script>
// Theme management
const THEME_KEY = 'nexus-docs-theme';
const themeToggle = document.getElementById('theme-toggle');
const sunIcon = themeToggle.querySelector('.theme-icon-sun');
const moonIcon = themeToggle.querySelector('.theme-icon-moon');

function applyTheme(theme) {{
    document.documentElement.setAttribute('data-theme', theme);
    if (theme === 'light') {{
        sunIcon.style.display = 'block';
        moonIcon.style.display = 'none';
    }} else {{
        sunIcon.style.display = 'none';
        moonIcon.style.display = 'block';
    }}
    localStorage.setItem(THEME_KEY, theme);
}}

function getPreferredTheme() {{
    const stored = localStorage.getItem(THEME_KEY);
    if (stored) return stored;
    return window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light';
}}

function initTheme() {{
    const theme = getPreferredTheme();
    applyTheme(theme);
}}

function toggleTheme() {{
    const current = document.documentElement.getAttribute('data-theme') || 'dark';
    applyTheme(current === 'dark' ? 'light' : 'dark');
}}

themeToggle.addEventListener('click', toggleTheme);

// Keyboard shortcut: Ctrl+Shift+L
document.addEventListener('keydown', (e) => {{
    if (e.ctrlKey && e.shiftKey && e.key === 'L') {{
        e.preventDefault();
        toggleTheme();
    }}
}});

// Listen for system theme changes
window.matchMedia('(prefers-color-scheme: dark)').addEventListener('change', (e) => {{
    if (!localStorage.getItem(THEME_KEY)) {{
        applyTheme(e.matches ? 'dark' : 'light');
    }}
}});

// Initialize theme immediately to prevent flash
initTheme();

// Mobile sidebar toggle
const sidebar = document.getElementById('sidebar');
const mobileMenuBtn = document.getElementById('mobile-menu-btn');

if (mobileMenuBtn) {{
    mobileMenuBtn.addEventListener('click', () => {{
        sidebar.classList.toggle('open');
    }});
}}

document.addEventListener('click', (e) => {{
    if (window.innerWidth <= 768) {{
        if (!sidebar.contains(e.target) && !e.target.closest('.header')) {{
            sidebar.classList.remove('open');
        }}
    }}
}});

// Smooth scroll for anchor links
document.querySelectorAll('a[href^="#"]').forEach(anchor => {{
    anchor.addEventListener('click', function(e) {{
        const target = document.querySelector(this.getAttribute('href'));
        if (target) {{
            e.preventDefault();
            target.scrollIntoView({{ behavior: 'smooth', block: 'start' }});
            if (window.innerWidth <= 768) {{
                sidebar.classList.remove('open');
            }}
        }}
    }});
}});

// Active link highlighting based on scroll position
const sections = document.querySelectorAll('.content h2, .content h3');
const sidebarLinks = document.querySelectorAll('.sidebar-link[href^="#"]');

function updateActiveLink() {{
    let current = '';
    sections.forEach(section => {{
        const rect = section.getBoundingClientRect();
        if (rect.top <= 100) {{
            current = '#' + section.id;
        }}
    }});

    sidebarLinks.forEach(link => {{
        link.classList.remove('active');
        if (link.getAttribute('href') === current) {{
            link.classList.add('active');
        }}
    }});
}}

window.addEventListener('scroll', updateActiveLink, {{ passive: true }});
window.addEventListener('load', updateActiveLink);

// Highlight.js initialization
if (typeof hljs !== 'undefined') {{
    hljs.highlightAll();
}}

// Mobile menu button visibility
const mobileMenuBtn = document.getElementById('mobile-menu-btn');
function checkMobile() {{
    if (window.innerWidth <= 768) {{
        mobileMenuBtn.style.display = 'flex';
    }} else {{
        mobileMenuBtn.style.display = 'none';
        document.getElementById('sidebar').classList.remove('open');
    }}
}}
checkMobile();
window.addEventListener('resize', checkMobile);

mobileMenuBtn.addEventListener('click', () => {{
    document.getElementById('sidebar').classList.toggle('open');
}});
</script>
'''

def process_markdown_file(md_path: Path, output_dir: Path) -> bool:
    """Process a single markdown file to HTML."""
    try:
        content = md_path.read_text(encoding='utf-8')
        
        # Extract title from first heading
        title_match = re.search(r'^#\s+(.+)$', content, re.MULTILINE)
        title = title_match.group(1) if title_match else md_path.stem.replace('-', ' ').title()
        
        # Convert to HTML
        html_content = markdown_to_html(content)
        
        # Determine output path
        rel_path = md_path.relative_to(DOCS_ROOT)
        output_path = output_dir / rel_path.with_suffix('.html')
        output_path.parent.mkdir(parents=True, exist_ok=True)
        
        # Generate page
        current_page = rel_path.with_suffix('.html').as_posix()
        full_html = generate_html_page(title, html_content, current_page)
        
        output_path.write_text(full_html, encoding='utf-8')
        print(f"  \u2713 {rel_path} \u2192 {output_path.relative_to(output_dir)}")
        return True
    except Exception as e:
        print(f"  \u2717 {md_path}: {e}")
        return False

def copy_assets():
    """Copy static assets to HTML output."""
    # Write CSS
    css_path = HTML_ROOT / "styles.css"
    css_path.write_text(CSS_CONTENT, encoding='utf-8')
    
    # Copy any images
    images_src = DOCS_ROOT / "images"
    if images_src.exists():
        images_dst = HTML_ROOT / "images"
        if images_dst.exists():
            shutil.rmtree(images_dst)
        shutil.copytree(images_src, images_dst)

def generate_index():
    """Generate the main index.html."""
    index_head = f'''<!DOCTYPE html>
<html lang="en" data-theme="dark">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Nexus Modeling Documentation</title>
    <link rel="stylesheet" href="styles.css">
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
    <link href="https://fonts.googleapis.com/css2?family=JetBrains+Mono:wght@400;500&display=swap" rel="stylesheet">
    <script src="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/highlight.min.js"></script>
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/styles/github-dark.min.css">
</head>
<body>
    <div class="main-content" style="margin-left: 0;">
        <header class="header">
            <button class="btn btn-icon" id="mobile-menu-btn" aria-label="Toggle menu" style="display: none;">
                <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
                    <line x1="3" y1="12" x2="21" y2="12"></line>
                    <line x1="3" y1="6" x2="21" y2="6"></line>
                    <line x1="3" y1="18" x2="21" y2="18"></line>
                </svg>
            </button>
            <div class="header-title">Nexus Modeling</div>
            <div class="header-actions">
                {THEME_TOGGLE_BTN}
                <a href="https://github.com/nexus-systems/nexus-modeling" class="btn btn-secondary" target="_blank" rel="noopener">
                    <svg width="18" height="18" viewBox="0 0 24 24" fill="currentColor">
                        <path d="M12 0C5.374 0 0 5.373 0 12c0 5.302 3.438 9.8 8.207 11.387.599.111.793-.261.793-.577v-2.234c-3.338.726-4.033-1.416-4.033-1.416-.546-1.387-1.333-1.756-1.333-1.756-1.089-.745.083-.729.083-.729 1.205.084 1.839 1.237 1.839 1.237 1.07 1.834 2.807 1.304 3.492.997.107-.775.418-1.305.762-1.604-2.665-.305-5.467-1.334-5.467-5.931 0-1.311.469-2.381 1.236-3.221-.124-.303-.535-1.524.117-3.176 0 0 1.008-.322 3.301 1.23A11.509 11.509 0 0112 5.803c1.02.005 2.047.138 3.006.404 2.291-1.552 3.297-1.23 3.297-1.23.653 1.653.242 2.874.118 3.176.77.84 1.235 1.911 1.235 3.221 0 4.609-2.807 5.624-5.479 5.921.43.372.823 1.102.823 2.222v3.293c0 .319.192.694.801.576C20.566 21.797 24 17.3 24 12c0-6.627-5.373-12-12-12z"/>
                    </svg>
                    <span class="btn-text">GitHub</span>
                </a>
            </div>
        </header>
        <main class="content" style="text-align: center; padding: 4rem 2rem;">
            <h1 style="font-size: 3.5rem; margin-bottom: 1rem; background: linear-gradient(135deg, var(--primary), var(--accent)); -webkit-background-clip: text; -webkit-text-fill-color: transparent; background-clip: text;">Nexus Modeling</h1>
            <p style="font-size: 1.5rem; color: var(--text-secondary); margin-bottom: 3rem; max-width: 600px; margin-left: auto; margin-right: auto;">
                Professional 3D Modeling & CAD Application \u2014 Building the alternative to Blender, Maya, 3ds Max, Modo, Houdini, Plasticity, and Rhino with a Vulkan-first C++26 kernel.
            </p>
            <div style="display: flex; gap: 1rem; justify-content: center; flex-wrap: wrap; margin-bottom: 4rem;">
                <a href="developer/architecture.html" class="btn btn-primary" style="font-size: 1.1rem; padding: 1rem 2rem;">Developer Docs</a>
                <a href="expert/advanced-modeling.html" class="btn btn-secondary" style="font-size: 1.1rem; padding: 1rem 2rem;">Expert Guides</a>
                <a href="beginner/getting-started.html" class="btn btn-secondary" style="font-size: 1.1rem; padding: 1rem 2rem;">Beginner Tutorials</a>
                <a href="api/kernel.html" class="btn btn-secondary" style="font-size: 1.1rem; padding: 1rem 2rem;">API Reference</a>
            </div>
            
            <div style="max-width: 1000px; margin: 0 auto; text-align: left;">
                <h2 style="margin-bottom: 1.5rem;">Project Status</h2>
                <table style="width: 100%;">
                    <thead>
                        <tr><th>Component</th><th>Status</th><th>Tests</th></tr>
                    </thead>
                    <tbody>
                        <tr><td>Geometry Kernel</td><td>\u2705 Core complete</td><td>1921/1921</td></tr>
                        <tr><td>Vulkan Renderer</td><td>\u2705 Headless + Windowed</td><td>145/145</td></tr>
                        <tr><td>Analytic B-rep</td><td>\u2705 5 primitives</td><td>2025/2026</td></tr>
                        <tr><td>Boolean/CSG</td><td>\u2705 Real CSG pipeline</td><td>2025/2026</td></tr>
                        <tr><td>Half-edge + Euler ops</td><td>\u2705 6/6 integrity-clean</td><td>2025/2026</td></tr>
                        <tr><td>Editor UI (Vulkan)</td><td>\u2705 ImGui + Vulkan</td><td>Working</td></tr>
                        <tr><td>Headless render-to-PNG</td><td>\u2705 <code>--shot</code> flag</td><td>Working</td></tr>
                        <tr><td><strong>Total Test Suite</strong></td><td colspan="2"><strong>2026 tests passing</strong></td></tr>
                    </tbody>
                </table>
            </div>
            
            <div style="margin-top: 4rem; padding-top: 2rem; border-top: 1px solid var(--border);">
                <h2>Documentation Sections</h2>
                <div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); gap: 1.5rem; margin-top: 1.5rem;">
                    <div style="background: var(--surface); border: 1px solid var(--border); border-radius: 8px; padding: 1.5rem;">
                        <h3 style="color: var(--primary); margin-bottom: 1rem;">\U0001F4DA Developer Documentation</h3>
                        <ul style="list-style: none; padding: 0;">
                            <li><a href="developer/architecture.html">Architecture Overview</a></li>
                            <li><a href="developer/geometry-kernel.html">Geometry Kernel Internals</a></li>
                            <li><a href="developer/renderer.html">Render System</a></li>
                            <li><a href="developer/editor-architecture.html">Editor Architecture</a></li>
                            <li><a href="developer/build-system.html">Build System</a></li>
                            <li><a href="developer/testing.html">Testing Strategy</a></li>
                            <li><a href="developer/boolean-csg.html">Boolean/CSG Deep Dive</a></li>
                            <li><a href="developer/halfedge-mesh.html">Half-edge Mesh</a></li>
                            <li><a href="developer/analytic-brep.html">Analytic B-rep</a></li>
                        </ul>
                    </div>
                    <div style="background: var(--surface); border: 1px solid var(--border); border-radius: 8px; padding: 1.5rem;">
                        <h3 style="color: var(--accent); margin-bottom: 1rem;">\U0001F3A8 Expert User Guides</h3>
                        <ul style="list-style: none; padding: 0;">
                            <li style="color: var(--text-muted);">Coming soon</li>
                        </ul>
                    </div>
                    <div style="background: var(--surface); border: 1px solid var(--border); border-radius: 8px; padding: 1.5rem;">
                        <h3 style="color: var(--primary); margin-bottom: 1rem;">\U0001F464 Beginner Tutorials</h3>
                        <ul style="list-style: none; padding: 0;">
                            <li><a href="beginner/getting-started.html">Getting Started</a></li>
                        </ul>
                    </div>
                    <div style="background: var(--surface); border: 1px solid var(--border); border-radius: 8px; padding: 1.5rem;">
                        <h3 style="color: var(--accent); margin-bottom: 1rem;">\U0001F527 API Reference</h3>
                        <ul style="list-style: none; padding: 0;">
                            <li style="color: var(--text-muted);">Coming soon</li>
                        </ul>
                    </div>
                    <div style="background: var(--surface); border: 1px solid var(--border); border-radius: 8px; padding: 1.5rem;">
                        <h3 style="color: var(--primary); margin-bottom: 1rem;">\U0001F4DA Additional Resources</h3>
                        <ul style="list-style: none; padding: 0;">
                            <li><a href="design-decisions.html">Design Decisions</a></li>
                            <li><a href="testing-strategy.html">Testing Strategy</a></li>
                            <li><a href="graphics-kernel.html">Graphics Kernel</a></li>
                            <li><a href="vision-and-roadmap.html">Vision & Roadmap</a></li>
                            <li><a href="reference-roadmap.html">Reference Roadmap</a></li>
                            <li><a href="modeling-industry-parity.html">Modeling Industry Parity</a></li>
                            <li><a href="modeling-parity-index.html">Modeling Parity Index</a></li>
                            <li><a href="modeling-reference-roadmap.html">Modeling Reference Roadmap</a></li>
                        </ul>
                    </div>
                </div>
            </div>
            
            <div style="margin-top: 4rem; padding-top: 2rem; border-top: 1px solid var(--border); color: var(--text-muted); font-size: 0.9rem;">
                <p>Nexus Modeling v0.1.0-dev | <a href="https://github.com/nexus-systems/nexus-modeling">GitHub</a> | Apache 2.0 License</p>
                <p>Last updated: 2026-07-14</p>
            </div>
        </main>
    </div>
'''
    
    index_js = '''
    <script>
        // Theme management
        const THEME_KEY = 'nexus-docs-theme';
        const themeToggle = document.getElementById('theme-toggle');
        const sunIcon = themeToggle.querySelector('.theme-icon-sun');
        const moonIcon = themeToggle.querySelector('.theme-icon-moon');

        function applyTheme(theme) {
            document.documentElement.setAttribute('data-theme', theme);
            if (theme === 'light') {
                sunIcon.style.display = 'block';
                moonIcon.style.display = 'none';
            } else {
                sunIcon.style.display = 'none';
                moonIcon.style.display = 'block';
            }
            localStorage.setItem(THEME_KEY, theme);
        }

        function getPreferredTheme() {
            const stored = localStorage.getItem(THEME_KEY);
            if (stored) return stored;
            return window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light';
        }

        function initTheme() {
            const theme = getPreferredTheme();
            applyTheme(theme);
        }

        function toggleTheme() {
            const current = document.documentElement.getAttribute('data-theme') || 'dark';
            applyTheme(current === 'dark' ? 'light' : 'dark');
        }

        themeToggle.addEventListener('click', toggleTheme);

        document.addEventListener('keydown', (e) => {
            if (e.ctrlKey && e.shiftKey && e.key === 'L') {
                e.preventDefault();
                toggleTheme();
            }
        });

        window.matchMedia('(prefers-color-scheme: dark)').addEventListener('change', (e) => {
            if (!localStorage.getItem(THEME_KEY)) {
                applyTheme(e.matches ? 'dark' : 'light');
            }
        });

        initTheme();

        // Mobile sidebar toggle
        const sidebar = document.getElementById('sidebar');
        const mobileMenuBtn = document.getElementById('mobile-menu-btn');

        if (mobileMenuBtn) {
            mobileMenuBtn.addEventListener('click', () => {
                sidebar.classList.toggle('open');
            });
        }

        document.addEventListener('click', (e) => {
            if (window.innerWidth <= 768) {
                if (!sidebar.contains(e.target) && !e.target.closest('.header')) {
                    sidebar.classList.remove('open');
                }
            }
        });

        // Smooth scroll for anchor links
        document.querySelectorAll('a[href^="#"]').forEach(anchor => {
            anchor.addEventListener('click', function(e) {
                const target = document.querySelector(this.getAttribute('href'));
                if (target) {
                    e.preventDefault();
                    target.scrollIntoView({ behavior: 'smooth', block: 'start' });
                    if (window.innerWidth <= 768) {
                        sidebar.classList.remove('open');
                    }
                }
            });
        });

        // Active link highlighting based on scroll position
        const sections = document.querySelectorAll('.content h2, .content h3');
        const sidebarLinks = document.querySelectorAll('.sidebar-link[href^="#"]');

        function updateActiveLink() {
            let current = '';
            sections.forEach(section => {
                const rect = section.getBoundingClientRect();
                if (rect.top <= 100) {
                    current = '#' + section.id;
                }
            });

            sidebarLinks.forEach(link => {
                link.classList.remove('active');
                if (link.getAttribute('href') === current) {
                    link.classList.add('active');
                }
            });
        }

        window.addEventListener('scroll', updateActiveLink, { passive: true });
        window.addEventListener('load', updateActiveLink);

        // Highlight.js initialization
        if (typeof hljs !== 'undefined') {
            hljs.highlightAll();
        }
        
        // Mobile menu button visibility
        const mobileMenuBtn = document.getElementById('mobile-menu-btn');
        function checkMobile() {
            if (window.innerWidth <= 768) {
                mobileMenuBtn.style.display = 'flex';
            } else {
                mobileMenuBtn.style.display = 'none';
                document.getElementById('sidebar').classList.remove('open');
            }
        }
        checkMobile();
        window.addEventListener('resize', checkMobile);
        
        mobileMenuBtn.addEventListener('click', () => {
            document.getElementById('sidebar').classList.toggle('open');
        });
    </script>
'''
    
    index_html = index_head + index_js + '</body>\n</html>'
    
    (HTML_ROOT / "index.html").write_text(index_html, encoding='utf-8')
    print("  \u2713 Generated index.html")

def main():
    """Main entry point."""
    print("Generating HTML documentation...")
    print(f"Source: {DOCS_ROOT}")
    print(f"Output: {HTML_ROOT}")
    
    # Clean and create output directory
    if HTML_ROOT.exists():
        shutil.rmtree(HTML_ROOT)
    HTML_ROOT.mkdir(parents=True)
    
    # Copy assets
    copy_assets()
    
    # Process all markdown files
    md_files = list(DOCS_ROOT.rglob("*.md"))
    # Filter out files in html directory and node_modules
    md_files = [f for f in md_files if 'html' not in f.parts and 'node_modules' not in f.parts]
    
    print(f"\nProcessing {len(md_files)} markdown files...")
    processed = 0
    failed = 0
    
    for md_file in md_files:
        if process_markdown_file(md_file, HTML_ROOT):
            processed += 1
        else:
            failed += 1
    
    # Generate index
    generate_index()
    
    print(f"\nDone! Processed: {processed}, Failed: {failed}")
    print(f"Output: {HTML_ROOT}")

if __name__ == "__main__":
    main()