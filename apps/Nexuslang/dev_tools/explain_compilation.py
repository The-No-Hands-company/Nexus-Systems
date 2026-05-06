#!/usr/bin/env python3
"""
NLPL Compilation Pipeline Visualizer
=====================================

Shows the different compilation paths from NexusLang to executables.
Demonstrates why we have multiple backends, not just one.
"""

from colorama import Fore, Style, init
init(autoreset=True)


def print_section(title):
    print(f"\n{Fore.CYAN}{'='*80}")
    print(f"{Fore.CYAN}{title}")
    print(f"{Fore.CYAN}{'='*80}\n")


def main():
    print_section("NLPL COMPILATION STRATEGIES")
    
    print(f"{Fore.YELLOW}The Question:")
    print(f"  'Why go NexusLang → C → Executable instead of NexusLang → Executable directly?'\n")
    
    print(f"{Fore.GREEN}The Answer:")
    print(f"  Because NexusLang supports MULTIPLE compilation targets for different use cases!\n")
    
    # Show the different paths
    print_section("PATH 1: SYSTEM APPLICATIONS (What We Built)")
    print(f"{Fore.WHITE}Use Case: {Fore.CYAN}Desktop apps, CLI tools, system utilities")
    print(f"\n  NexusLang Source (.nxl)")
    print(f"    ↓ {Fore.YELLOW}[C Code Generator]")
    print(f"  C Source (.c)")
    print(f"    ↓ {Fore.YELLOW}[GCC/Clang Compiler]")
    print(f"  Machine Code")
    print(f"    ↓ {Fore.YELLOW}[Linker]")
    print(f"  {Fore.GREEN} Native Executable{Style.RESET_ALL}")
    
    print(f"\n{Fore.MAGENTA}Why this path?")
    print(f"  • GCC has 40+ years of optimization")
    print(f"  • Supports all CPU architectures (x86, ARM, RISC-V, etc.)")
    print(f"  • Portable across operating systems")
    print(f"  • Proven, reliable, battle-tested")
    
    # Path 2: Web Applications
    print_section("PATH 2: WEB APPLICATIONS (Future)")
    print(f"{Fore.WHITE}Use Case: {Fore.CYAN}Browser apps, Node.js servers, React apps")
    print(f"\n  NexusLang Source (.nxl)")
    print(f"    ↓ {Fore.YELLOW}[JavaScript Generator]")
    print(f"  JavaScript (.js)")
    print(f"    ↓ {Fore.YELLOW}[Browser/Node.js Runtime]")
    print(f"  {Fore.GREEN} Running Web App{Style.RESET_ALL}")
    
    print(f"\n{Fore.MAGENTA}Why this path?")
    print(f"  • No compilation needed - instant deployment")
    print(f"  • Access to DOM, Web APIs")
    print(f"  • npm ecosystem integration")
    print(f"  • Same language for frontend + backend")
    
    # Path 3: OS Kernels
    print_section("PATH 3: OPERATING SYSTEM KERNELS (Advanced Future)")
    print(f"{Fore.WHITE}Use Case: {Fore.CYAN}Linux kernel, bootloaders, device drivers")
    print(f"\n  NexusLang Source (.nxl)")
    print(f"    ↓ {Fore.YELLOW}[Assembly Generator]")
    print(f"  Assembly (.asm)")
    print(f"    ↓ {Fore.YELLOW}[Assembler]")
    print(f"  Machine Code")
    print(f"    ↓ {Fore.YELLOW}[Linker - NO C Runtime]")
    print(f"  {Fore.GREEN} Kernel Binary{Style.RESET_ALL}")
    
    print(f"\n{Fore.MAGENTA}Why this path?")
    print(f"  • OS kernels run BEFORE C runtime exists")
    print(f"  • Need direct hardware access (port I/O, interrupts)")
    print(f"  • Can't depend on libc or any external libraries")
    print(f"  • MUST generate raw assembly")
    
    # Path 4: Production Apps
    print_section("PATH 4: PRODUCTION APPLICATIONS (Priority Future)")
    print(f"{Fore.WHITE}Use Case: {Fore.CYAN}Commercial software, high-performance apps")
    print(f"\n  NexusLang Source (.nxl)")
    print(f"    ↓ {Fore.YELLOW}[LLVM IR Generator]")
    print(f"  LLVM Intermediate Representation")
    print(f"    ↓ {Fore.YELLOW}[LLVM Optimizer - 100+ passes]")
    print(f"  Optimized LLVM IR")
    print(f"    ↓ {Fore.YELLOW}[LLVM Backend]")
    print(f"  Machine Code")
    print(f"    ↓ {Fore.YELLOW}[Linker]")
    print(f"  {Fore.GREEN} Highly Optimized Executable{Style.RESET_ALL}")
    
    print(f"\n{Fore.MAGENTA}Why this path?")
    print(f"  • Professional-grade optimization (used by Rust, Swift)")
    print(f"  • Portable IR, native execution")
    print(f"  • Can target multiple architectures from one IR")
    print(f"  • Better than C backend for production code")
    
    # Comparison Table
    print_section("COMPARISON: WHY MULTIPLE BACKENDS?")
    
    print(f"{Fore.WHITE}{'Backend':<15} {'Compile Time':<15} {'Binary Size':<15} {'Best For'}")
    print(f"{'-'*80}")
    print(f"{Fore.CYAN}C Backend       {Fore.GREEN}~100ms          {Fore.GREEN}~16 KB          {Fore.YELLOW}Quick prototyping")
    print(f"{Fore.CYAN}C++ Backend     {Fore.GREEN}~150ms          {Fore.GREEN}~18 KB          {Fore.YELLOW}OOP applications")
    print(f"{Fore.CYAN}JavaScript      {Fore.GREEN}~50ms           {Fore.GREEN}N/A (runtime)   {Fore.YELLOW}Web applications")
    print(f"{Fore.CYAN}LLVM Backend    {Fore.YELLOW}~200ms          {Fore.GREEN}~14 KB          {Fore.YELLOW}Production apps")
    print(f"{Fore.CYAN}Assembly        {Fore.GREEN}~50ms           {Fore.GREEN}~8 KB           {Fore.YELLOW}OS kernels")
    print(f"{Fore.CYAN}WASM            {Fore.YELLOW}~500ms          {Fore.GREEN}~10 KB          {Fore.YELLOW}Universal binaries")
    
    # Real-World Examples
    print_section("REAL-WORLD ANALOGY")
    
    print(f"{Fore.YELLOW}Think of compilation like transportation:\n")
    
    print(f"  {Fore.CYAN}Going to the grocery store?")
    print(f"    → Walk (JavaScript: no compilation, just run)")
    
    print(f"\n  {Fore.CYAN}Going to work?")
    print(f"    → Drive (C backend: reliable, fast enough)")
    
    print(f"\n  {Fore.CYAN}Going across the country?")
    print(f"    → Fly (LLVM: worth the extra setup for speed)")
    
    print(f"\n  {Fore.CYAN}Going to Mars?")
    print(f"    → Rocket (Assembly: when nothing else will work)")
    
    print(f"\n{Fore.GREEN}You don't use a rocket to go to the grocery store!")
    print(f"{Fore.GREEN}You don't walk to Mars!")
    print(f"{Fore.GREEN}Different tasks need different tools.\n")
    
    # The Key Insight
    print_section("THE KEY INSIGHT")
    
    print(f"{Fore.YELLOW}NLPL's C backend is NOT a compromise or limitation.")
    print(f"{Fore.YELLOW}It's the FIRST of MANY backends we'll build.\n")
    
    print(f"{Fore.GREEN}Each backend serves a specific purpose:")
    print(f"  • {Fore.CYAN}C/C++{Fore.WHITE}      → Rapid development, portability")
    print(f"  • {Fore.CYAN}JavaScript{Fore.WHITE} → Web applications")
    print(f"  • {Fore.CYAN}LLVM{Fore.WHITE}       → Production-quality optimization")
    print(f"  • {Fore.CYAN}Assembly{Fore.WHITE}   → OS kernels, bare metal")
    print(f"  • {Fore.CYAN}WASM{Fore.WHITE}       → Universal execution")
    
    print(f"\n{Fore.MAGENTA}This is how professional languages work:")
    print(f"  • Kotlin: JVM + JavaScript + Native")
    print(f"  • Haskell: Native + JavaScript (GHCJS)")
    print(f"  • Scala: JVM + JavaScript + Native")
    print(f"  • Nim: C + C++ + JavaScript + Objective-C")
    
    # The Vision
    print_section("NLPL'S VISION")
    
    print(f"{Fore.CYAN}One day, you'll write:\n")
    
    print(f"{Fore.WHITE}  # Same NexusLang source code")
    print(f"  {Fore.YELLOW}set message to \"Hello, World!\"")
    print(f"  {Fore.YELLOW}print text message\n")
    
    print(f"{Fore.CYAN}And compile it ANYWHERE:\n")
    
    print(f"  {Fore.GREEN}$ nlpl compile app.nlpl --target c          {Fore.WHITE}# Linux CLI tool")
    print(f"  {Fore.GREEN}$ nlpl compile app.nlpl --target js         {Fore.WHITE}# React web app")
    print(f"  {Fore.GREEN}$ nlpl compile app.nlpl --target wasm       {Fore.WHITE}# Browser game")
    print(f"  {Fore.GREEN}$ nlpl compile app.nlpl --target llvm       {Fore.WHITE}# Production server")
    print(f"  {Fore.GREEN}$ nlpl compile app.nlpl --target asm-x64    {Fore.WHITE}# Linux kernel module")
    print(f"  {Fore.GREEN}$ nlpl compile app.nlpl --target asm-arm    {Fore.WHITE}# Raspberry Pi firmware")
    
    print(f"\n{Fore.YELLOW}{'='*80}")
    print(f"{Fore.YELLOW}ONE LANGUAGE. EVERY PLATFORM. EVERY USE CASE.")
    print(f"{Fore.YELLOW}{'='*80}\n")


if __name__ == "__main__":
    main()
