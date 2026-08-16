# Adapt CPU and memory profile

Status: measured 2026-08-16 against the current Qwen3 0.6B Q8 preview runtime.

This document records the resource cost of Adapt so model, context, and runtime
changes do not silently change SAID's hardware requirements. Update it whenever
the Adapt model artifact, context size, batch size, generated-token limit, or
thread policy changes.

## User-facing recommendation

- **Practical minimum:** 8 GB of system RAM for SAID and ordinary desktop apps.
- **Recommended for Adapt:** 16 GB of system RAM.
- **Recommended for development:** 32 GB when containers, virtual machines, or
  large local builds run beside SAID.

The 16 GB recommendation provides headroom for the operating system and the
application receiving dictation. Adapt itself does not allocate 16 GB.

## Current footprint

| State | CPU | Resident memory |
| --- | ---: | ---: |
| Exact or Clean | No Adapt inference | Base SAID reference: about 564 MiB idle |
| Adapt selected, before first use | No Adapt inference | Base SAID only; Qwen3 is lazy-loaded |
| Adapt generating | 5.6–6.7 logical cores on the reference CPU | About 1.10 GiB for the Adapt engine |
| Adapt warm, between requests | Effectively zero; the worker blocks | About 1.10 GiB remains resident for Adapt |

Combining the measured Adapt engine with the existing speech stack gives an
estimated **1.65 GiB complete SAID working set while Adapt is warm**. This is an
additive estimate rather than a native Windows whole-process measurement.

The stopped-process resident snapshot broke the Adapt engine down as follows:

| Memory class | Measurement |
| --- | ---: |
| Peak RSS | 1,153,104 KiB / 1,126 MiB |
| Proportional set size | 1,150,204 KiB / 1,123 MiB |
| File-backed resident pages | 626,284 KiB / 612 MiB |
| Private anonymous memory | 526,820 KiB / 515 MiB |
| Virtual address space | 1,517,624 KiB / 1,482 MiB |
| Swap | 0 |
| Runtime threads | 8 |

The 639,446,688-byte GGUF artifact accounts for most file-backed pages. The
private allocation is primarily runtime working memory for the 4,096-token
context and batches. The model and context remain allocated after a request so
the next Adapt pass can start without reloading them.

## CPU and latency results

Linux GNU `time` reports aggregate CPU, where 100% means one fully occupied
logical processor.

| Workload | Median wall time | Average CPU | Notes |
| --- | ---: | ---: | --- |
| Technical developer fixture, fresh process | 1.36 s | 558% / 5.58 logical cores | Five isolated runs; the safety gate kept the Clean input |
| Four-pass built-in warm benchmark | 3.07 s total | 673% / 6.73 logical cores | Five isolated processes |
| Slowest warm pass inside each benchmark | 0.79 s median | — | Observed range: 0.639–0.905 s |

The production worker waits on a condition variable when its queue is empty, so
keeping Adapt warm retains memory but does not intentionally poll or consume CPU.
Inference duration still scales with prompt and generated-output length. The
eight-thread cap can occupy a larger share of a smaller processor than the
17–21% aggregate-machine share observed on the 32-thread reference host.

## Measurement environment

- Intel Core i9-14900HX, 32 logical processors.
- 16 GiB system RAM.
- Linux 6.18 under WSL2 on the Windows development host.
- Release build (`-O3 -DNDEBUG`) of `said_grammar_file`.
- Pinned llama.cpp `b6114`, native CPU backend, OpenMP disabled.
- `Qwen3-0.6B-Q8_0.gguf`, SHA-256
  `9465e63a22add5354d9bb4b99e90117043c7124007664907259bd16d043bb031`.
- 4,096-token context, 4,096-token batch, 512-token micro-batch, greedy
  decoding, and the automatic eight-thread cap.
- Model file and filesystem metadata were already in the operating-system page
  cache. Each measured workload still started a new process and rebuilt its
  model context.

The supported release is the native Windows x64 build. These Linux/WSL2 results
exercise SAID's real `LocalGrammarCorrector` and are suitable for requirements
and regression tracking, but they are directional rather than a native Windows
hardware certification. The Windows build also disables `GGML_NATIVE`, so CPU
timing can differ even when memory remains similar.

## Reproducing the measurement

Configure the portable model probe in Release mode, then build
`said_grammar_file`:

```sh
cmake -S . -B build-model-probe \
  -DCMAKE_BUILD_TYPE=Release \
  -DSAID_BUILD_MODEL_PROBE=ON
cmake --build build-model-probe --target said_grammar_file --parallel
```

Run the four-pass warm workload under GNU `time` at least five times, without
other CPU-heavy builds running:

```sh
/usr/bin/time -f 'wall_s=%e cpu=%P user_s=%U sys_s=%S max_rss_kb=%M' \
  ./build-model-probe/said_grammar_file \
  /path/to/Qwen3-0.6B-Q8_0.gguf \
  --benchmark-developer \
  'please add regression tests for the login bug'
```

Record medians and ranges, and inspect `/proc/<pid>/smaps_rollup` during a warm
pass when the resident/private split is needed. Resource profiling is separate
from model-quality acceptance: a safety fallback still incurs the inference
cost before SAID decides to keep Clean.

## Maintenance checklist

When a runtime change can affect footprint:

1. Rebuild the Release probe from the exact candidate revision.
2. Run at least five isolated processes after the machine becomes idle.
3. Record RSS, PSS, private/file-backed pages, CPU, wall time, and thread count.
4. Recalculate the complete-app estimate using a current native Windows base
   working-set measurement.
5. Update this file, the README hardware section, and the Adapt choice in
   `src/setup_window.cpp` if the recommendation or rounded warm footprint
   changes.
