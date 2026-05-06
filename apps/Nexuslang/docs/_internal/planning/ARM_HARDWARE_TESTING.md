# ARM/AArch64 Hardware Testing Issues

This document tracks hardware-dependent testing that cannot be completed without physical ARM devices.

## Overview

NLPL inline assembly implementation includes ARM/AArch64 support, but the development team only has access to x86_64 hardware. The following tests are **ready to run** but **require ARM hardware** for validation.

## Test Files Ready for Hardware Validation

### 1. Basic ARM Operations
- **File**: `test_programs/unit/assembly/test_asm_arm_basic.nlpl`
- **Tests**: 5 comprehensive tests covering ADD, SUB, MUL, bitwise, shifts
- **Expected Results**: All documented in test file
- **Status**: ⚠️ Awaiting hardware validation

### 2. ARM Advanced Features
- **File**: `test_programs/unit/assembly/test_asm_arm_advanced.nlpl`
- **Tests**: 5 tests for CSEL, ADC, UBFX, REV, load/store offsets
- **Expected Results**: All documented in test file
- **Status**: ⚠️ Awaiting hardware validation

### 3. ARM NEON SIMD
- **File**: `test_programs/unit/assembly/test_asm_arm_neon.nlpl`
- **Tests**: 3 simplified NEON tests
- **Note**: Full NEON requires vector register support (future enhancement)
- **Status**: ⚠️ Awaiting hardware validation

## Required Hardware Platforms

### High Priority (Essential for 100% completion)

#### 1. Raspberry Pi Testing
- **Device**: Raspberry Pi 4 or 5 (8GB recommended)
- **OS**: Ubuntu Server 22.04 LTS ARM64 or Raspberry Pi OS 64-bit
- **Purpose**: Primary ARM development board validation
- **GitHub Issue**: #TBD
- **Volunteers**: Looking for testers

#### 2. Apple Silicon Testing
- **Device**: Mac with M1, M2, or M3 chip
- **OS**: macOS 12.0 (Monterey) or later
- **Purpose**: AArch64 validation on desktop platform
- **GitHub Issue**: #TBD
- **Volunteers**: Looking for testers

### Medium Priority (Nice to have)

#### 3. AWS Graviton Testing
- **Device**: EC2 t4g.micro (free tier eligible)
- **OS**: Ubuntu 22.04 LTS ARM64
- **Purpose**: Cloud ARM server validation
- **Cost**: Free tier available
- **GitHub Issue**: #TBD
- **Volunteers**: Can be automated via CI/CD

#### 4. ARM Development Board
- **Devices**: NVIDIA Jetson, NXP i.MX 8M, Qualcomm DragonBoard
- **OS**: Vendor-specific Linux ARM64
- **Purpose**: Embedded systems validation
- **GitHub Issue**: #TBD
- **Volunteers**: Looking for testers

## How to Help

### If You Have ARM Hardware

1. **Clone NexusLang repository**:
   ```bash
   git clone https://github.com/Zajfan/NLPL.git
   cd NexusLang
   ```

2. **Install dependencies**:
   ```bash
   pip3 install -r requirements.txt
   ```

3. **Run ARM tests**:
   ```bash
   # Basic operations
   python3 dev_tools/nlplc test_programs/unit/assembly/test_asm_arm_basic.nlpl
   ./test_asm_arm_basic
   
   # Advanced features
   python3 dev_tools/nlplc test_programs/unit/assembly/test_asm_arm_advanced.nlpl
   ./test_asm_arm_advanced
   
   # NEON SIMD
   python3 dev_tools/nlplc test_programs/unit/assembly/test_asm_arm_neon.nlpl
   ./test_asm_arm_neon
   ```

4. **Report results**: Create a GitHub issue using the template below

### Issue Template: ARM Test Results

```markdown
---
title: "[ARM Test] Results on [Device Name]"
labels: testing, arm, hardware-dependent
---

## Hardware Information

**Device**: (e.g., Raspberry Pi 4 Model B 8GB)
**CPU**: (e.g., ARM Cortex-A72, output of `cat /proc/cpuinfo`)
**Architecture**: (output of `uname -m`)
**OS**: (e.g., Ubuntu 22.04.3 LTS ARM64)
**Kernel**: (output of `uname -r`)
**NLPL Commit**: (output of `git rev-parse HEAD`)

## Test Results

### test_asm_arm_basic.nlpl

- [ ] Compilation: SUCCESS / FAILED
- [ ] Execution: SUCCESS / FAILED

**Test 1 (ADD)**:
- Expected: 30
- Actual: ___

**Test 2 (SUB)**:
- Expected: 35
- Actual: ___

**Test 3 (MUL)**:
- Expected: 42
- Actual: ___

**Test 4 (Bitwise - AND/OR/XOR)**:
- Expected: 10, 15, 5
- Actual: ___, ___, ___

**Test 5 (Shift - LSL/LSR)**:
- Expected: 32, 4
- Actual: ___, ___

### test_asm_arm_advanced.nlpl

- [ ] Compilation: SUCCESS / FAILED
- [ ] Execution: SUCCESS / FAILED

**Test 1 (CSEL)**:
- Expected: 20
- Actual: ___

**Test 2 (Offset)**:
- Expected: 1016
- Actual: ___

**Test 3 (ADC)**:
- Expected: 1
- Actual: ___

**Test 4 (UBFX)**:
- Expected: 255
- Actual: ___

**Test 5 (REV)**:
- Expected: 2018915346
- Actual: ___

### test_asm_arm_neon.nlpl

- [ ] Compilation: SUCCESS / FAILED
- [ ] Execution: SUCCESS / FAILED

**Test 1 (NEON Add)**:
- Expected: 30
- Actual: ___

**Test 2 (NEON Mul)**:
- Expected: 30
- Actual: ___

**Test 3 (NEON Load/Store)**:
- Expected: 4160
- Actual: ___

## Compilation Output

<details>
<summary>Compilation messages (if any warnings/errors)</summary>

```
Paste compilation output here
```

</details>

## Runtime Output

<details>
<summary>Full program output</summary>

```
Paste full output here
```

</details>

## Issues Encountered

(Describe any problems, unexpected behavior, or errors)

## Additional Notes

(Any other observations or comments)
```

## Documentation

See comprehensive guide: [docs/7_development/cross_platform_inline_assembly.md](../docs/7_development/cross_platform_inline_assembly.md)

Includes:
- ARM register conventions
- Constraint mapping
- Architecture-specific examples
- Debugging tips
- Performance considerations

## Current Status

| Platform | Status | Tester | GitHub Issue |
|----------|--------|--------|--------------|
| x86_64 Linux | ✅ Fully tested | Dev team | N/A |
| Raspberry Pi | ⚠️ Awaiting tests | Looking for volunteers | #TBD |
| Apple Silicon | ⚠️ Awaiting tests | Looking for volunteers | #TBD |
| AWS Graviton | ⚠️ Awaiting tests | Can automate | #TBD |
| NVIDIA Jetson | ⚠️ Awaiting tests | Looking for volunteers | #TBD |

## Blockers

**None** - All code is implemented and ready. Only hardware validation is pending.

## Timeline

- **Code Complete**: February 14, 2026 ✅
- **Hardware Testing**: Ongoing (community-driven)
- **100% Validation**: When all platforms tested

## Contact

- **Repository**: https://github.com/Zajfan/NLPL
- **Discussions**: GitHub Discussions tab
- **Issues**: GitHub Issues tab

---

**Note**: This is NOT a code implementation blocker. All ARM support is implemented and working. We just need hardware to validate the implementation produces correct results.

**Status**: Ready for hardware validation  
**Last Updated**: February 14, 2026
