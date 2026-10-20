#!/usr/bin/env python3
"""Freeze the two Core27-targeted fixture inputs; does not compile or execute."""
import argparse
import hashlib
import json
from pathlib import Path


def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def emit(p, value):
    p.write_text(json.dumps(value, indent=2, sort_keys=True) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    source = root / 'tests/ep_l2/ms_victim/test_vrw_reachable.cc'
    cases = {
        'split128': dict(readonly=[0x400000], writes=[],
            operations=['One128B GLOBAL parent A0 enters actual sector frontend',
                        'Observe4actual RO lower packets,4shared requester descriptors',
                        'One32B duplicate A0 sector0 merges; expect5descriptors and no new lower',
                        'Return actual lower sectors3,1,2,0; check four childUID/original-parent pointers and one ordinary duplicate UID',
                        'Check no parentUID response, descriptor/pin/token/epoch drain; reclaim external parent after its4children'],
            expected=dict(RO_lower_packets=4, RO_lower_bytes=128, total_lower_read_packets=4,
                          total_lower_read_bytes=128, shared_requester_descriptors=5,
                          descriptors_after_first_three_child_responses=2, child_responses=4,
                          ordinary_duplicate_responses=1, parent_responses=0, final_descriptors=0)),
        'joint32': dict(readonly=[0x400000+i*8192 for i in range(17)], writes=[i*8192 for i in range(144)],
            operations=['Native TEXTURE read A0 sector0 with real lower return',
                        'Write B0..16 sector0; GLOBAL A0 read is V hit before R, no lower',
                        'Write B17..143 sector0;128mandatory WAD rows reclaim optional A0',
                        'For i=0..16: complete actual WB B(32+i), submit32B RO Ai, hold lower',
                        'Select smallest i whose actual original handle is RO_FILL_DETACHED and no longer bound to Tag',
                        'Repeat that Ai sector0; new handle and one actual paid duplicate lower',
                        'Covered host update attempt must refuse before mutation',
                        'Choose retained inflight WB B0..31 excluding B4 on different bank from selected oldRO',
                        'Submit victim read; one explicit charged public-arbiter contender denies read bank at oldRO fill cycle',
                        'Old RO final return at N; its real enqueue at N+1 fills capacity1ICNT ahead of victim retry',
                        'WB done while victim reader pinned; preserve token/pin4cycles; consume ICNT, last-unpin TVD_VALID',
                        'Write B4 waits real priorWB completion and native Tag promotion; return remaining RO packets',
                        'Finish all WB one per cycle; check updated B4, exact-once replies and shared drain'],
            expected=dict(RO_lower_packets=18, RO_lower_bytes=576, native_lower_read_packets=1,
                          total_lower_read_packets=19, total_lower_read_bytes=608,
                          shared_requester_descriptors=18, mandatory_rows_before_RO=128,
                          final_descriptors=0, final_pending_bank_operations=0))}
    for name, data in cases.items():
        folder = out / name
        folder.mkdir()
        assert set(data['readonly']).isdisjoint(data['writes'])
        stimulus = dict(schema='B_VRW_CORE27_REACHABLE_STIMULUS_V2', case=name,
            source_path=str(source.relative_to(root)), source_sha256=sha(source),
            scope='ENTIRE_FINITE_SYNTHETIC_FIXTURE_EPOCH_NOT_CAPTURED_APPLICATION',
            readonly_lines=[f'{x:016x}' for x in data['readonly']],
            write_lines=[f'{x:016x}' for x in data['writes']], operations=data['operations'],
            repeated_write_line='0000000000008000' if name == 'joint32' else None,
            host_attempt='REFUSED_NOT_EXECUTED_WRITE' if name == 'joint32' else None,
            frozen_policy='reuse_seen remains permanent; no false full-parent detach assumption',
            independent_C_scope_review='PENDING')
        emit(folder / 'STIMULUS.json', stimulus)
        (folder / 'kernelslist.g').write_text(f'B-VRW-review-v2-{name}-whole-synthetic-epoch.fixture\n')
        (folder / 'ranges.csv').write_text('first_line,last_line_inclusive\n' +
            ''.join(f'{x:016x},{x:016x}\n' for x in data['readonly']))
        manifest = dict(schema='EP_L2_RO_ORACLE_V1', status='SYNTHETIC_DIRECTED_FIXTURE',
            epoch='whole_commandlist', scope='TRACE_MODEL_CLOSED_WORLD',
            address_domain='CANONICAL_GPU_GLOBAL_VA_NO_TRANSLATION_ALIAS', unresolved_opcodes={},
            commandlist_sha256=sha(folder / 'kernelslist.g'),
            artifacts={'certified_ranges.csv': sha(folder / 'ranges.csv')},
            synthetic_stimulus_sha256=sha(folder / 'STIMULUS.json'),
            scientific_qualification='NONE_PENDING_C_REVIEW_A_EXECUTION_E_ACCEPTANCE')
        emit(folder / 'manifest.json', manifest)
        config = (root / 'tests/ep_l2/ft_wbuf.config').read_text()
        config += (root / 'tests/ep_l2/ms_victim/dirty_full.config').read_text()
        config += ('\n-gpgpu_ep_l2_ro_tag_detach 1\n-gpgpu_ep_l2_ro_line_mshr_bypass 1\n'
                   '-gpgpu_ep_l2_ro_ranges ranges.csv\n'
                   f'-gpgpu_ep_l2_ro_ranges_sha256 {sha(folder / "ranges.csv")}\n'
                   '-gpgpu_ep_l2_ro_manifest manifest.json\n'
                   f'-gpgpu_ep_l2_ro_manifest_sha256 {sha(folder / "manifest.json")}\n'
                   '-gpgpu_ep_l2_ro_scope TRACE_MODEL_CLOSED_WORLD\n')
        (folder / 'combined.config').write_text(config)
        plan = dict(schema='B_VRW_CORE27_REACHABLE_NATIVE_PLAN_V2', case=name,
            A_Framework_review='156adbbb5ddcd49e8adb96939837372936ac451e',
            production_Core_preimage='27ec10bda848e4929181c60a7cbb40d6b007945c',
            production_main_sha256='773271ac70f009dddcd4c522f66dcd37967e4f57ce1e5082d3fc5aea61f2f9f5',
            fixture_source_sha256=sha(source), fixture_binary_sha256=None,
            status='PREPARED_NOT_COMPILED_NOT_RUN_C_REVIEW_PENDING',
            actual_native_runs_by_B=0, observed_counts=None, expected_only=data['expected'],
            cwd='THIS_CASE_DIRECTORY',
            argv=['A_FROZEN_TEST_BUILD/ep_l2_ms_vrw_reachable',
                  'A_CORE27/configs/tested-cfgs/SM7_QV100/gpgpusim.config',
                  'combined.config', 'kernelslist.g', '16', '0', '0', '1', name],
            base_config_sha256=sha(root / 'configs/tested-cfgs/SM7_QV100/gpgpusim.config'),
            overrides=dict(geometry='S:64:128:16,L:B:m:L:L,A:128:32,256:0,32',
                           queues='512:2:64:1', payloads=1168, tags=1024, descriptors=512,
                           native_lines=128, WAD_TVD_rows=128, banks=4, ops_per_bank_cycle=1),
            execution_owner='A_UNIFIED_INTEGRATION_E_ADMISSION_AND_INDEPENDENT_ACCEPTANCE',
            scope_limit='Symbolic nonzero sector reference and constructed paid contention; not nativeDRAM/SASS or performance qualification',
            old_failure_retained=True, change_production_protocol=False, reset_E_qualification=False)
        emit(folder / 'PLAN.json', plan)
        (folder / 'SHA256SUMS').write_text(''.join(f'{sha(p)}  {p.name}\n' for p in sorted(folder.iterdir()) if p.is_file()))
    print(json.dumps(dict(status='PREPARED_ONLY', cases=list(cases), actual_native_runs_by_B=0)))


if __name__ == '__main__':
    main()
