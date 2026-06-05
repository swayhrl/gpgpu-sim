# Mascar W18B Guidance: Default-Off Kernel Launch Trace in Simulator

## Stage position

This is W18B.

W18A audited existing logs/source. If existing logs do not provide enough kernel launch name/order, W18B adds default-off simulator trace.

## Goal

Add a default-off kernel launch trace facility that prints structured kernel begin/end lines with launch order, kernel name, grid, block, and optional cycles.

This trace is for paper reproduction and phase mapping only. It must not change simulation behavior by default.

## Hard constraints

1. Default behavior must remain unchanged.
2. New trace must be controlled by config knobs.
3. Do not modify M1-M4 Mascar mechanism behavior.
4. Do not spam unlimited logs.
5. Do not make trace required for normal runs.
6. Do not break builds when kernel name/grid fields are unavailable.
7. Keep trace best-effort; if end-cycle stats are hard, begin trace is still useful.
8. Build must pass before W18C.

## Config knobs

Add knobs to the appropriate simulator config class after inspecting source.

Prefer global simulator config, not shader_core_config, because kernel launch is global.

Suggested names:

- int gpgpu_paperrepro_kernel_trace
- unsigned gpgpu_paperrepro_kernel_trace_max
- int gpgpu_paperrepro_kernel_trace_stats

Suggested defaults:

- gpgpu_paperrepro_kernel_trace = 0
- gpgpu_paperrepro_kernel_trace_max = 4096
- gpgpu_paperrepro_kernel_trace_stats = 0

Meaning:
- trace disabled by default
- trace_max bounds total trace lines
- trace_stats optionally prints begin/end cycle/stat deltas if feasible

If adding a string output file option is easy and consistent with code style, it may be added. Otherwise print to simulator stdout/log.

## Trace line format

Print stable machine-readable lines.

Required begin line:

paperrepro_kernel_begin launch_index=<N> uid=<UID> name=<NAME> grid=(<X>,<Y>,<Z>) block=(<X>,<Y>,<Z>) stream=<S> cycle=<C>

Required end line if feasible:

paperrepro_kernel_end launch_index=<N> uid=<UID> name=<NAME> cycle=<C>

Optional fields if available:

shmem=<N>
nregs=<N>
cta=<N>
threads=<N>
kernel_uid=<N>

Do not include unescaped commas inside values if possible.

If name is unavailable, use name=unknown.
If uid is unavailable, use uid=0 or blank.
If grid/block are unavailable, use blank or zero.

## Where to instrument

Do not guess blindly. Search code first.

Likely source concepts:
- kernel_info_t
- stream_manager
- gpgpu_sim::launch
- active kernel list
- kernel completion path
- grid/block dim accessors
- kernel name accessor

Use grep:

grep -RIn "kernel_info_t\|stream_manager\|launch\|grid_dim\|block_dim\|get_name\|name()" src/gpgpu-sim src/cuda-sim src 2>/dev/null | head -n 300

Instrument the place where each kernel is accepted into the simulator or launched on GPU.

For end line, instrument the place where kernel completion is observed. If completion path is unclear, skip end line and document.

## Trace line emission helper

Add a small helper function to avoid duplicated formatting.

The helper should:
- check config enabled
- check trace count < max
- increment trace count
- print line
- not allocate heavily
- not assert on missing fields

## Config directories

Create:

- configs/hrl-repro/SM7_QV100_mascar_kernel_trace_baseline_off/

Base it on:
- configs/hrl-repro/SM7_QV100_mascar_baseline_off/

Add trace knobs:
- -gpgpu_paperrepro_kernel_trace 1
- -gpgpu_paperrepro_kernel_trace_max 4096
- -gpgpu_paperrepro_kernel_trace_stats 1 if implemented

Create README.md:
- explains default-off kernel trace
- purpose is phase mapping
- not for performance comparison
- no Mascar active behavior

Optional:
- configs/hrl-repro/SM7_QV100_mascar_kernel_trace_m4_reexec_load_on/
Only create if cheap; baseline trace is enough for phase mapping.

## Collector update

Update:

- experiments/common/gpgpusim_matrix/collect_kernel_trace.py

It must parse paperrepro_kernel_begin/end lines.

Create or update:
- experiments/common/gpgpusim_matrix/README.md section for kernel trace if README exists.

## Documentation

Create:

- docs/papers/mascar_w18b_default_off_kernel_trace.md

Required sections:
1. Goal
2. Config knobs
3. Source instrumentation points
4. Trace format
5. Parser support
6. Safety and default-off behavior
7. Limitations
8. How W18C will use it

## Validation

Run:

- git diff --check
- source setup_environment release && make -j2
- grep gpgpu_paperrepro_kernel_trace
- grep paperrepro_kernel_begin
- python3 -m py_compile experiments/common/gpgpusim_matrix/collect_kernel_trace.py
- dry-run a wrapper with trace config if feasible

## Stop conditions

Stop W18B only if:
1. build cannot be restored
2. trace instrumentation requires broad simulator redesign
3. trace changes normal behavior when disabled

Do not stop because end trace is hard. Begin trace is sufficient for first phase mapping.
