#!/usr/bin/env python3
"""Prepare exact finite native-test inputs, no compiler/test/app invocation."""
import argparse
import hashlib
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--base-config', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    core = Path(__file__).resolve().parents[3]
    if args.out.exists():
        # Rebuilding unpublished planning metadata is allowed. Never overwrite
        # a raw/native result, executable, or a plan that says it was run.
        existing=args.out/'NATIVE_TEST_PLAN.json'
        assert existing.is_file() and not json.loads(existing.read_text())['identity']['native_tests_executed']
        assert all(p.name in {'off.config','clean.config','dirty_inflight.config','NATIVE_TEST_PLAN.json'} for p in args.out.iterdir())
    args.out.mkdir(parents=True, exist_ok=True)
    base = args.base_config.resolve()
    assert base.is_file()
    identity = {'base_config': str(base), 'base_config_sha256': hashlib.sha256(base.read_bytes()).hexdigest(),
                'parent_core': '84359d4ba492acb639a2092fa271e9169038bf7d',
                'A_interface': '980072bbbcbd4ad00cdf85650283931e94cad395',
                'runtime_framework_parent': '197741be9be9b8e390a557d639503b416d8e95f0',
                'vendor_pybind11': 'f90c430c3a1ce8f08c09942a5a1c3a797bb2295b',
                'built_binary': None, 'native_tests_executed': False, 'full_apps': 0,
                'required_resource_admission': 'E fresh campaign lease plus48GiB workspace40GiB Share64GiB RAM; no bypass',
                'maximum_parallel_jobs': 1, 'small_module_budget': {'cpu': 1, 'memory_bytes': 268435456, 'output_bytes': 8388608},
                'production_fixture_budget_pending_A_build': {'cpu': 1, 'memory_bytes': 2147483648, 'output_bytes': 268435456}}
    common = core / 'tests/ep_l2/ft_wbuf.config'
    configs = {}
    for name in ('off', 'clean', 'dirty_inflight'):
        overlay = core / ('tests/ep_l2/ms_victim/' + name + '.config')
        content = common.read_text() + '\n' + overlay.read_text()
        target = args.out / (name + '.config')
        target.write_text(content)
        configs[name] = {'path': str(target.resolve()), 'sha256': hashlib.sha256(content.encode()).hexdigest(),
                         'ordered_sources': [str(common), str(overlay)]}
    cases = [dict(name='directory_clean',status='NOT_COMPILED_NOT_RUN', executable='B_BOUND_tvd_directory', arguments=['--clean'],
                  scope='directory+native payload arbiter component, not cache response or DRAM data proof'),
             dict(name='directory_dirty',status='NOT_COMPILED_NOT_RUN',executable='B_BOUND_tvd_directory',arguments=[],
                  scope='same component; actual cache/DRAM lifecycle checked by separate native fixtures')]
    for profile, case in [('off', 'cycle'), ('clean', 'cycle'), ('off', 'stream'), ('clean', 'stream'),
                          ('clean', 'sectors'), ('clean', 'partial'), ('dirty_inflight', 'dirty')]:
        cases.append({'name': profile + '__' + case, 'status': 'NOT_COMPILED_NOT_RUN',
                      'executable': 'A_BOUND_native_victim_fixture',
                      'arguments': [str(base), configs[profile]['path'], '2', '16', '0', '0', '1', case],
                      'scope': 'real1024Tag1168payloadD512 partition; controlled external memory versions, not native DRAM timing'})
    for selector in ('--study-clean', '--study-dirty-inflight'):
        cases.append({'name': selector, 'status': 'NOT_COMPILED_NOT_RUN', 'executable': 'A_BOUND_native_tvd_production',
                      'arguments': [str(base), configs['off']['path'], selector],
                      'scope': 'native partition/simpleDRAM; tiny and primary geometries labelled in source'})
    sources = ['src/gpgpu-sim/ep-l2-tvd-directory.h', 'src/gpgpu-sim/ep-l2-tvd-cache.inc',
               'tests/ep_l2/test_tvd_directory.cc', 'tests/ep_l2/test_tvd_production.cc',
               'tests/ep_l2/ms_victim/test_native.cc', 'tests/ep_l2/ms_victim/native_fixture.h']
    identity['sources'] = {p: hashlib.sha256((core / p).read_bytes()).hexdigest() for p in sources}
    (args.out / 'NATIVE_TEST_PLAN.json').write_text(json.dumps(dict(identity=identity, configs=configs, cases=cases), indent=2)+'\n')
    print(json.dumps(dict(status='PREPARED_ONLY', native_executions=0, planned_processes=len(cases), out=str(args.out))))


if __name__ == '__main__':
    main()
