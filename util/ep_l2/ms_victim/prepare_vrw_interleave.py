#!/usr/bin/env python3
"""Prepare ONE synthetic whole-epoch V/R/W input; never build or execute it.

The input is a complete C++ fixture footprint, not a captured application trace.
C must review the certificate's scope and A must integrate the public paths.
Output must be new; this program refuses to overwrite a frozen revision.
"""
import argparse
import hashlib
import json
from pathlib import Path


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    stride = 64 * 128
    ro_lines = [0x400000 + i * stride for i in range(17)]
    write_lines = [i * stride for i in range(144)]
    # Complete synthetic epoch includes every writer, the repeated B4 writer,
    # and a refused host update (no write performed). No opcode heuristic.
    assert set(ro_lines).isdisjoint(write_lines)
    stimulus = dict(
        schema='B_C_VRW_SYNTHETIC_WHOLE_EPOCH_V1',
        source='tests/ep_l2/ms_victim/test_vrw_interleave.cc',
        source_sha256=sha(root / 'tests/ep_l2/ms_victim/test_vrw_interleave.cc'),
        readonly_lines=[f'{a:016x}' for a in ro_lines],
        write_lines=[f'{a:016x}' for a in write_lines],
        operations=[
            'native TEXTURE read A0 sector0, real lower return',
            'write B0..B16 sector0; retain A0 CLEAN and B0 WB',
            'GLOBAL read A0 sector0: V before R, no new lower read',
            'write B17..B143 sector0; 128 mandatory rows preempt clean A0',
            'complete B32; GLOBAL read A0 all4 sectors, hold actual lower packets',
            'for i=1..16: complete B(32+i), GLOBAL read Ai sector0, hold lower',
            'GLOBAL read A0 sector0: independent duplicate after Tag detach',
            'attempt covered host update A0: must refuse before any mutation',
            'return old A0 sectors3,1,2; choose real WB victim with different bank, excluding B4',
            'read selected WB; one explicit paid public-arbiter contender; old A0 final fill0',
            'hold ICNT capacity1; WB done while V read pinned; consume and check final state',
            'write B4 sector0: must wait WB done, then promotion; return remaining RO packets reverse',
            'complete remaining WB one per cycle; read updated B4, drain epoch and flush optional victims'],
        repeated_write_line=f'{4 * stride:016x}',
        host_update=dict(line=f'{ro_lines[0]:016x}', must_be_refused=True, actual_write=False),
        certification_status='PENDING_C_WHOLE_SYNTHETIC_EPOCH_REVIEW_NOT_APP_CERTIFICATE',
        lower_memory='EXTERNAL_NONZERO_SECTOR_VERSION_REFERENCE_NOT_NATIVE_DRAM_TIMING_OR_SASS_VALUES')
    write_json(out / 'STIMULUS.json', stimulus)
    (out / 'kernelslist.g').write_text('B-C-VRW-whole-synthetic-epoch-v1.fixture\n')
    (out / 'ranges.csv').write_text('first_line,last_line_inclusive\n' +
                                  ''.join(f'{a:016x},{a:016x}\n' for a in ro_lines))
    manifest = dict(schema='EP_L2_RO_ORACLE_V1', status='SYNTHETIC_DIRECTED_FIXTURE',
                    epoch='whole_commandlist', scope='TRACE_MODEL_CLOSED_WORLD',
                    address_domain='CANONICAL_GPU_GLOBAL_VA_NO_TRANSLATION_ALIAS',
                    unresolved_opcodes={}, commandlist_sha256=sha(out / 'kernelslist.g'),
                    artifacts={'certified_ranges.csv': sha(out / 'ranges.csv')},
                    synthetic_stimulus_sha256=sha(out / 'STIMULUS.json'),
                    scientific_qualification='NONE_PENDING_C_SCOPE_REVIEW_AND_A_E_NATIVE_VALIDATION')
    write_json(out / 'manifest.json', manifest)
    # Relative paths deliberately bind CWD to this frozen input directory.
    # The future A/E runner must check all file hashes and set that CWD.
    overlay = (root / 'tests/ep_l2/ft_wbuf.config').read_text()
    overlay += (root / 'tests/ep_l2/ms_victim/dirty_full.config').read_text()
    overlay += ('\n-gpgpu_ep_l2_ro_tag_detach 1\n-gpgpu_ep_l2_ro_line_mshr_bypass 1\n'
                '-gpgpu_ep_l2_ro_ranges ranges.csv\n'
                f'-gpgpu_ep_l2_ro_ranges_sha256 {sha(out / "ranges.csv")}\n'
                '-gpgpu_ep_l2_ro_manifest manifest.json\n'
                f'-gpgpu_ep_l2_ro_manifest_sha256 {sha(out / "manifest.json")}\n'
                '-gpgpu_ep_l2_ro_scope TRACE_MODEL_CLOSED_WORLD\n')
    (out / 'combined.config').write_text(overlay)
    base = root / 'configs/tested-cfgs/SM7_QV100/gpgpusim.config'
    plan = dict(schema='B_C_VRW_NATIVE_INPUT_PLAN_V1',
                coordination='b2e5328aca825d6e9ebfe09e46fa208957834636',
                A_victim_preimage='6452b74d92c2cf72f80e5f5e102941d6cf9adf7a',
                A_RO_preimage='aa2a92cc11ef0f22f9f2a6f3418ce31bdee33278',
                integrated_source=None, binary_sha256=None, effective_dump=None,
                status='PREPARED_NOT_COMPILED_NOT_RUN_PENDING_C_REVIEW',
                native_executions=0, observed_hits=None, observed_lower_reads=None,
                full_apps_started_by_B=0,
                budget=dict(campaign_total_limit=40, additional_40=False, writer='E_ONLY'),
                cwd='DIRECTORY_CONTAINING_THIS_PLAN',
                argv=['A_FROZEN_BUILD/ep_l2_ms_vrw_interleave', 'A_FROZEN_CORE/configs/tested-cfgs/SM7_QV100/gpgpusim.config',
                      'combined.config', 'kernelslist.g', '16', '0', '0', '1'],
                base_config_sha256=sha(base),
                fixture_overrides=dict(geometry='S:64:128:16,L:B:m:L:L,A:128:32,256:0,32',
                                       queues='512:2:64:1', ft_mode=2, wbuf=16, static_s64=0, local_cap=0, bank_ops=1),
                expected_only=dict(lower_read_packets=22, lower_read_bytes=704,
                                   RO_lower_packets=21, RO_lower_bytes=672,
                                   old_plus_duplicate_A0_sector_packets=5, pending_shared_descriptors=18,
                                   mandatory_rows_at_pressure=128,
                                   shared_descriptors_at_drain=0, bank_ops_max_per_physical_cycle=1),
                remaining=['A combine V-before-RO frontend and host guard before any victim mutation',
                           'C review complete synthetic footprint and RO/native four-head arbiter identity/mux',
                           'A integrate exact last-unpin fix and opt-in test; A/E real admitted build and effective dump',
                           'E independent execution/acceptance; constructed contender is not observed app contention',
                           'Separate native DRAM timing/issues and scalar/SASS numerical qualification are not this fixture'])
    write_json(out / 'NATIVE_INPUT_PLAN.json', plan)
    (out / 'SHA256SUMS').write_text(''.join(f'{sha(p)}  {p.name}\n' for p in sorted(out.iterdir()) if p.is_file()))
    print(json.dumps(dict(status=plan['status'], native_executions=0, prepared_files=7)))


if __name__ == '__main__':
    main()
