# Zero-destination global atomic lifecycle repair

Parent: `2e3bef4364e578cddd0e881c0baace2e51c55930`.
Authority: the researcher explicitly requested isolated repair and validation
after the 2026-10-25 full-workload failure diagnosis. This worktree does not
modify any active/frozen source, runtime library, input, original attempt or
historical result ledger. Runtime geometry/options are unchanged.

## Defect and repair

The source decoder represents both zero-destination RED and ATOMG as global
LOAD_OP atomics using CACHE_GLOBAL. M4 owns one external dependency/PIB entry,
but register-driven writeback_complete returns false for zero outputs, so the
old completion callback never released that dependency. Architectural atomic
counts could nevertheless finish. This caused a source-localized finite-PIB
stall (BFS) or a terminal nonempty-PIB assertion (hybridsort/histo). Exact
historical residual owner inventories were not printed; these diagnoses do
not invent UID/count evidence.

M4 now records each admitted global atomic's active lane mask under its UID.
Only successful real GLOBAL mem_fetch writebacks consume returned access lane
masks, the same masks used by source architectural atomic-count release.
The source atomic coalescer assigns each active lane to one transaction and
asserts the access fits a single segment. Overlapping, empty, foreign-lane or
already-completed returns assert. Only the final distinct return closes the
external dependency exactly once, independently of output registers.
Dispatch does not close it. Existing atomic side effects, coalescing, request
route, register writeback and architecture completion remain in place.
Shared/zero-active atomics do not allocate this sidecar.

Kernel drain is a separate correction: pending PIB/create/issue/inflight and
response work keeps GPU/cluster/core service running after architectural
threads end. Admission records each IO/OO entry's owning kernel UID; a done
kernel is deferred only while its own entries remain. finished_kernel then
publishes the ordinary completed record after this finite tail. Unrelated
kernels need not drain first. Retained valid Ref0 cache data are not pending
work. Existing finite retire and lower service widths stay unchanged.

## CPU regression

`dtc_l1_m4_atomic_completion_test` uses actual libcudart LDST global mem_fetch
WB, configured frontends, scoreboard, architectural atomic counters and real
core->cycle/drain/finished_kernel logic. No kernel/trace executes and there
are no GPU cycle() calls or runtime threads. The access-control compiler flag
is only a fixture setup seam, not a production compile flag.

Six CTest variants cover LEGACY, PAPER_BASE, IO, OO AE=0/1, MODERN_OO_SECTOR.
Controls cover zero-destination RED/ATOMG source shape, returning atomics,
multiple/disjoint/out-of-order lane replies, interleaved UIDs, repeated
admission, early/duplicate/empty/foreign-lane completion rejection, zero-active
and shared exclusion, actual finite IO256/OO128 capacity and one-per-cycle
retirement, threads-ended pending drain, kernel-specific completion and
retained valid Ref0 non-busyness. Existing DTC+matched-hash tests also pass
(18 total registered tests). Assertions remain enabled in Release.

Build: CMake Release, GCC11.4, source-bound CUDA12.4 header/toolchain package,
`GPGPUSIM_BUILD_DTC_L1_TESTS=ON`, `GPGPUSIM_BUILD_AEFAIR_TESTS=ON`; isolated
binary directory `/root/data/dtc_failure_repair_20261025_v1/core-build`.
Final post-commit build/test receipts, source/binary SHA and bundle remain
outside Git in the repair root/control. Raw logs/traces/binaries are not
committed.

## Result validity

This is a behavior/timing correction, not a counter-only edit. Affected old
atomic/sidecar results do not become accepted through this patch. Preserve
all historical failed and accepted rows with their original behavior IDs.
New full-input repair validation and required VecAdd/mixed-path scientific
sentinels must use new immutable source/binary identities and their own
epoch2 request/ledger. This Core task starts no scientific simulator jobs.
