#!/usr/bin/env python3
"""Read existing GDB/fixture output only; not a runtime or science acceptance API."""
import argparse
import hashlib
import json
from pathlib import Path


def records(text, marker):
    # Native stdout can have a partial line when GDB stops it. Preserve the
    # real record after that prefix; never require the marker at column zero.
    decoder = json.JSONDecoder()
    result = []
    for line in text.splitlines():
        if marker in line:
            value, end = decoder.raw_decode(line.partition(marker)[2])
            if not isinstance(value, dict):
                raise ValueError('observation must be a JSON object')
            result.append(value)
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('raw_log', type=Path)
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args()
    raw = args.raw_log.read_bytes()
    rows = records(raw.decode(), 'B_DRAM_ACCEPT_R4|')
    exits = records(raw.decode(), 'B_DRAM_NATIVE_EXIT_R4|')
    if len(rows) != 2 or exits != [dict(exit_code=0, capture_hits=2)]:
        raise ValueError('missing native completion or pair of observations')
    for i, row in enumerate(rows):
        if (row.get('status') != 'OBSERVED' or row.get('on') != i or
                row.get('iteration_order') != i or
                row.get('scope') != 'READ_ONLY_EXISTING_FIXTURE_FIFO_REMOVAL_MAP'):
            raise ValueError('wrong observation identity/scope/order')
        pairs = row['read_uid_issue_counts']
        if (len({uid for uid, count in pairs}) != len(pairs) or
                any(type(uid) is not int or uid <= 0 or count != 1 for uid, count in pairs) or
                not row['all_read_UIDs_exactly_once'] or
                sum(count for uid, count in pairs) != row['actual_simple_DRAM_read_accepts']):
            raise ValueError('UID issue counts do not close')
    if [r['actual_simple_DRAM_read_accepts'] for r in rows] != [2, 1]:
        raise ValueError('frozen OFF/CLEAN fixture expectation failed')
    if len({(r['inferior_pid'], r['proc_start_ticks'], r['boot_id']) for r in rows}) != 1:
        raise ValueError('observations have different native process identities')
    output = dict(schema='B_FROZEN_NATIVE_DRAM_ACCEPT_PARSE_R4',
        status='OBSERVED_2_TO_1_FINITE_FROZEN_ELF_NOT_E_ACCEPTANCE',
        raw_log=str(args.raw_log.resolve()), raw_sha256=hashlib.sha256(raw).hexdigest(),
        observations=rows, native_exit=exits[0],
        scope='Actual fixed-latency simpleDRAM FIFO read acceptances; not detailed DRAM/HW or app performance',
        no_new_native_run=True, no_qualification_mutation=True)
    with args.output.open('x') as stream:
        json.dump(output, stream, indent=2)
        stream.write('\n')
    print(json.dumps(dict(status=output['status'], observed_accepts=[2, 1])))


if __name__ == '__main__':
    main()
