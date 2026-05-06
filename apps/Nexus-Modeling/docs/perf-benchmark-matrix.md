# Perf Benchmark Matrix

This document records the reproducible performance smoke matrix for the kernel.

## Purpose

The goal is not final performance tuning. The goal is reproducible baseline capture
so contributors can detect obvious regressions while the kernel is still forming.

## Harness

The baseline harness is:

1. CTest target: `KernelPerfSmoke.Null`
2. Executable: `nexus_kernel_perf_smoke`
3. Current scenario: `single_triangle_direct_path`

## Reproduction Commands

From repository root:

1. Configure: `cmake -S . -B build`
2. Build: `cmake --build build -j$(nproc)`
3. Run smoke: `./build/tests/nexus_kernel_perf_smoke --frames 120 --output build/kernel_perf_smoke.txt`

## Matrix

| Backend | Scenario | Frames | Source of truth | Status |
|---|---|---:|---|---|
| Null | single_triangle_direct_path | 120 | `build/kernel_perf_smoke.txt` | Automated |
| Vulkan | single_triangle_direct_path | 120 | Manual capture on GPU-capable runner | Pending environment |

## Initial Recorded Baseline

Recorded locally on 2026-05-06 using:

1. `./build/tests/nexus_kernel_perf_smoke --frames 120 --output build/kernel_perf_smoke.txt`

Observed output:

1. `backend=null`
2. `scenario=single_triangle_direct_path`
3. `frames=120`
4. `total_ms=0.012603`
5. `average_ms=0.000105`
6. `median_ms=0.000073`
7. `final_frame_draw_calls=0`

## Interpretation Rules

1. Treat this as a trend baseline, not a shipping performance guarantee.
2. Compare same backend, same frame count, same scenario.
3. Record notable regressions in PR notes when smoke timing changes materially.

## Month 1 Acceptance Link

This document is the reproducible benchmark matrix artifact referenced by
[vision-and-roadmap.md](vision-and-roadmap.md).