# ARM/AArch64 Test Suite

## Overview

This directory contains inline assembly tests specifically for ARM/AArch64 architectures. These tests are **ready to run** on ARM hardware but **cannot be executed on x86_64**.

## Test Files

### Basic Operations
- **test_asm_arm_basic.nlpl**: Fundamental ARM instructions
  - ADD, SUB, MUL operations
  - Bitwise operations (AND, ORR, EOR)
  - Shift operations (LSL, LSR, ASR)
  - Expected results documented in comments

### Advanced Features
- **test_asm_arm_advanced.nlpl**: ARM-specific capabilities
  - Conditional execution (CSEL)
  - Load/store with offsets
  - Add with carry (ADC)
  - Bit field operations (UBFX)
  - Byte reversal (REV)

### SIMD Operations
- **test_asm_arm_neon.nlpl**: NEON vector instructions
  - Vector addition
  - Vector multiplication
  - Vector load/store operations
  - Note: Simplified tests (full NEON requires vector register support)

## Hardware Requirements

These tests require ARM/AArch64 hardware:

### ✅ Supported Platforms

1. **Raspberry Pi** (AArch64 mode)
   - Raspberry Pi 3/4/5 with 64-bit OS
   - Ubuntu Server ARM64 or Raspberry Pi OS 64-bit

2. **Apple Silicon Mac**
   - M1, M2, M3, M3 Pro, M3 Max, M3 Ultra
   - macOS 11.0 (Big Sur) or later
   - Native ARM mode (not Rosetta 2)

3. **ARM Servers**
   - AWS Graviton (t4g, m6g, c6g, r6g instances)
   - Oracle Cloud ARM instances
   - Microsoft Azure Ampere instances
   - Google Cloud Tau T2A instances

4. **Development Boards**
   - NVIDIA Jetson (Nano, Xavier, Orin)
   - Qualcomm DragonBoard
   - NXP i.MX 8M Plus

### ❌ NOT Supported

- **x86_64 machines** (Intel/AMD processors)
- **QEMU emulation** (untested, may work with full system emulation)
- **32-bit ARM** (ARMv7) - may work but untested

## Running the Tests

### On Raspberry Pi

```bash
# 1. Clone NexusLang repository
git clone https://github.com/yourusername/NLPL.git
cd NexusLang

# 2. Install dependencies
pip3 install -r requirements.txt

# 3. Compile test
python3 dev_tools/nlplc test_programs/unit/assembly/test_asm_arm_basic.nlpl

# 4. Run test
./test_asm_arm_basic

# 5. Expected output
=== ARM/AArch64 Inline Assembly Tests ===

Test 1: Basic ADD operation
Result: 30 (expected: 30)

Test 2: Basic SUB operation
Result: 35 (expected: 35)

Test 3: Basic MUL operation
Result: 42 (expected: 42)

...
=== All ARM basic tests completed ===
```

### On Apple Silicon Mac

```bash
# 1. Verify native ARM mode
arch  # Should output: arm64

# 2. Clone and setup (same as Raspberry Pi)
git clone https://github.com/yourusername/NLPL.git
cd NexusLang
pip3 install -r requirements.txt

# 3. Run tests (same commands as Raspberry Pi)
python3 dev_tools/nlplc test_programs/unit/assembly/test_asm_arm_basic.nlpl
./test_asm_arm_basic
```

### On AWS Graviton

```bash
# 1. Launch ARM instance
# Instance type: t4g.micro (free tier eligible)
# OS: Ubuntu 22.04 LTS ARM64

# 2. SSH and setup
ssh ubuntu@<instance-ip>
sudo apt update
sudo apt install python3 python3-pip git
git clone https://github.com/yourusername/NLPL.git
cd NexusLang
pip3 install -r requirements.txt

# 3. Run tests
python3 dev_tools/nlplc test_programs/unit/assembly/test_asm_arm_basic.nlpl
./test_asm_arm_basic
```

## Test Results

### Expected Outputs

**test_asm_arm_basic.nlpl**:
- Test 1 (ADD): 30
- Test 2 (SUB): 35
- Test 3 (MUL): 42
- Test 4 (AND): 10, OR: 15, XOR: 5
- Test 5 (LSL): 32, LSR: 4

**test_asm_arm_advanced.nlpl**:
- Test 1 (CSEL): 20
- Test 2 (Offset): 1016
- Test 3 (ADC): 1
- Test 4 (UBFX): 255
- Test 5 (REV): 2018915346

**test_asm_arm_neon.nlpl**:
- Test 1: 30 (simplified)
- Test 2: 30 (simplified)
- Test 3: 4160 (simplified)

## Troubleshooting

### Issue: "Architecture not supported"

**Solution**: Verify you're on ARM hardware:
```bash
python3 -c "import platform; print(platform.machine())"
# Should show: aarch64, arm64, or armv8
```

### Issue: Compilation fails with "unknown instruction"

**Possible causes**:
1. Wrong architecture detected
2. LLVM not configured for ARM target
3. Invalid ARM instruction syntax

**Debug steps**:
```bash
# Enable debug mode
python3 dev_tools/nlplc --debug test_asm_arm_basic.nlpl

# Check LLVM IR generated
cat build/test_asm_arm_basic.ll | grep -A 10 "call.*asm"
```

### Issue: Wrong results on x86_64

**This is expected!** These tests contain ARM-specific instructions that don't exist on x86_64. You'll see:
- Compilation errors (good - validation working)
- Runtime errors (if it somehow compiles)
- Incorrect results (if x86_64 instructions happen to match)

**Solution**: Run tests on actual ARM hardware.

## Contributing

If you have ARM hardware and can help test:

1. **Run all three test files**
2. **Report results** with:
   - Hardware: Device model, CPU info
   - OS: Version and distribution
   - Actual vs expected outputs
   - Any compilation errors

3. **Create GitHub issue** with template:
   ```markdown
   ## ARM Test Results
   
   **Hardware**: Raspberry Pi 4 Model B (8GB)
   **CPU**: ARM Cortex-A72 (ARMv8-A)
   **OS**: Ubuntu 22.04.3 LTS ARM64
   **Kernel**: 5.15.0-1040-raspi
   
   ### test_asm_arm_basic.nlpl
   - [ ] Test 1 (ADD): Expected 30, Got ___
   - [ ] Test 2 (SUB): Expected 35, Got ___
   - [ ] Test 3 (MUL): Expected 42, Got ___
   - [ ] Test 4 (Bitwise): Expected 10/15/5, Got ___
   - [ ] Test 5 (Shift): Expected 32/4, Got ___
   
   ### test_asm_arm_advanced.nlpl
   - [ ] All tests passed: YES / NO
   - Issues: (if any)
   
   ### test_asm_arm_neon.nlpl
   - [ ] All tests passed: YES / NO
   - Issues: (if any)
   ```

## Known Limitations

1. **NEON tests are simplified**: Full NEON vector operations require vector register type support (not yet implemented)

2. **No cross-compilation**: Currently, NexusLang must be run on the target architecture

3. **Limited instruction coverage**: Only basic ARM instructions tested (no Crypto, SVE, etc.)

4. **No 32-bit ARM testing**: Tests are for AArch64 only

## Future Work

- [ ] Cross-compilation support (compile on x86_64, run on ARM)
- [ ] Full NEON vector register support
- [ ] SVE (Scalable Vector Extension) tests
- [ ] ARM Crypto extension tests
- [ ] 32-bit ARM (ARMv7) test suite
- [ ] CI/CD integration with ARM runners

---

**Status**: Ready for hardware testing  
**Last Updated**: February 14, 2026  
**Maintainer**: NexusLang Development Team

**Need Help?** See [docs/7_development/cross_platform_inline_assembly.md](../../../docs/7_development/cross_platform_inline_assembly.md) for comprehensive cross-platform guide.
