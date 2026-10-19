# Paid resources, timing and evidence scope

R2 update: `R2_DESIGN_AND_VALIDATION.md` and test_plan_r2 are current; the
earlier clean/inflight plan remains frozen in test_plan_r1. No native test ran.

| Item | Implementation / status |
|---|---|
| Data | Sole existing1168 slots ×128B, four banks. No victim data array. Existing four symbolic sector-version words per slot model data, not numerical SASS byte values. |
| Tags | Existing1024. Victim reads do not allocate another Tag. Primary native fixture uses64×16; historical tiny geometries are separate component cases. |
| Directory | Sole finite128 WAD/TVD rows, EMPTY/CLEAN/WB_STAGED/WB_INFLIGHT/WB_COMPLETE. Mandatory WB persists until set_done. Legacy WAD address set is unused while TVD is active. |
| Metadata fields | Line address,11-bit slot identity premise plus existing unsigned generation,3-bit state premise,4-bit valid/dirty sector masks, reader count, directory incarnation, retention age. Source uses software unsigned/uint64 fields; hardware widths and mux/CAM PPA are not inferred from sizeof. Born/lower timestamps and counters are observation fields and separately listed. |
| Query | Extra1 physical GPU cycle after Tag miss, ≤1 victim query/slice/cycle; row ticket cached through retries. One additional head ticket/done/cycle state. Mandatory WAD completion versus query/update scheduling still requires A review; no claim of free multiport metadata. |
| Read lease | At most one active frontend lease in the existing serialized request head, no second descriptor pool. Real payload pin until successful response enqueue; selected row/generation, mask and forward classification retained. |
| Bank | Actual TVD data read goes through the existing arbiter, same bank=id%4, P1. Directory hit counter is committed only after actual response enqueue. |
| Responses | Existing ICNT FIFO; no free response data buffer. Backpressure can keep pin and pending grant; retries need a real later grant/consume. |
| WB packet | Existing lower FIFO's independent version/data snapshot; local retained slot remains paid. WAD lives beyond local lower acceptance. |
| Statistics | served_sector_bytes is useful service volume, NOT avoided DRAM bytes. Compare actual lower/DRAM issue counters OFF/ON. Unmeasured waits and missing application results stay UNKNOWN. |
| Fixed baseline | R2 source reserves16 victim/276 other slots per bank (64/1104 total), even when empty. Reciprocal capacity quotas with same pointer mapping, not address-fixed IDs; one retention credit. NOT_COMPILED_NOT_RUN, no superiority claim. |

## Prepared finite tests — all NOT_COMPILED_NOT_RUN

1. Reused directory/production-arbiter component groups: clean lease/reclaim,
   stale ticket/sector/promotion/shared128; separately dirty snapshot, WAD
   priority, completion with pinned lease and full-row promotion. Test-only
   byte arrays in these component tests are an oracle, not actual cache data
   proof. Native cache tests below close that different requirement.
2. Native1024Tag/1168slot/D512 cache+partition with independently produced
   **nonzero sector versions**, injected through actual lower and ReturnQ:
   cyclic17 lines in one16way set (OFF/ON), streaming17 no-reuse (OFF/ON), four
   lower sectors returned3/1/0/2 then victim read, missing sector -> actual lower
   fallback and retired generation, dirty-inflight snapshot/wait/done/promotion.
   P1, query count, response exact-once, lower packet/byte counter consistency,
   directory/payload/pending drain are checked by the fixture.
3. Reused native partition/simple-DRAM `--study-clean`: real ICNT backpressure,
   bank conflict, missing sector, write/atomic promotion, primary geometry and
   matched actual lower reads/bytes PLUS actual simple-DRAM read issue counts.
   This timing fixture uses explicit version-zero lower memory; it does not
   replace the independent nonzero-version cache checks.
4. `--study-dirty-inflight`: native WB forward, write/atomic wait, partial-sector
   fallback and directory/lower-full resume. Further completion while bank-
   denied reader pinned, old packet after slot reuse, mandatory-by-bank reclaim
   and all applicable host epoch cases still require exact integrated execution.

Expected positive `cycle` source assertions OFF51/ON17 lower packets are **not
results**. Serve-in-place can change the later hit trajectory: only2 victim
responses in this sequence can avoid34 baseline lower reads. Thus served bytes
cannot be relabelled as DRAM savings. Stream expects17/17 and zero victim hits;
preserve that negative case. Actual DRAM issue reduction is checked separately
in the native simple-DRAM fixture, not assumed from external-memory facade.

The tools prepare source/config/hash/argv and a textual patch only. No test can
run until E's scientific resource pass and a correct same-source native build.
Small module estimate:1CPU/256MiB, <8MiB output. A-integrated fixture estimate:
1CPU/2GiB, <256MiB output; final command/archives require A's exact release.
These are estimates, not an admission waiver or a promised old-archive ABI.

No full applications, new traces, oracle changes, numerical partial-byte merge
or atomic arithmetic proof, multi-slice performance, or new PPA are claimed.
