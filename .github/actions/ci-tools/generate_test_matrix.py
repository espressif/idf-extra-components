#!/usr/bin/env python3
"""Plan test jobs from successful build results and manifest permissions."""

import argparse
import json
import os
from pathlib import Path


def collect_test_cases(eligible_apps):
    """Select cases only when every required app/target/config is eligible."""
    from idf_ci import get_pytest_cases

    if not eligible_apps:
        return []
    # Match the worker's collection mode, including Linux node IDs. Collecting
    # all targets also keeps multi-DUT cases visible for worker validation.
    cases = get_pytest_cases(target='all', marker_expr='', additional_args=['--suppress-no-test-exit-code'])
    selected = []
    for case in cases:
        if case.apps and not case.get_skip_reason_if_not_built(eligible_apps):
            selected.append(case)
        else:
            print(f'Excluding {case.item.nodeid}: application/configuration not successfully built or testing disabled by manifest')
    return selected


# Capability env markers map to dedicated self-hosted runner labels of this
# repo's runner pool. 'generic' deliberately adds nothing: any runner with
# the target label qualifies, and the pool has no 'generic' label — requesting
# it would leave jobs queued forever. Capability runners are dedicated boards
# identified by their capability label only, without the chip label.
RUNNER_CAPABILITY_LABELS = {
    'ethernet': ('ESP32-ETHERNET-KIT',),
    'spi_nand_flash': ('spi_nand_flash',),
    'quad_psram': ('quad_psram',),
    'usb_serial_jtag': ('usb_serial_jtag',),
}


def runner_labels(case, target, execution):
    # QEMU runs on GitHub-hosted Linux (no KVM dependency); host tests too.
    if execution in ('linux', 'qemu'):
        return ['ubuntu-24.04']
    if execution != 'hardware':
        raise ValueError(f'Unsupported test execution profile: {execution}')
    unknown = set(case.env_markers) - set(RUNNER_CAPABILITY_LABELS) - {'generic'}
    if unknown:
        raise ValueError(f'No runner label mapping for env markers: {sorted(unknown)}')
    capabilities = [label for marker in sorted(case.env_markers)
                    for label in RUNNER_CAPABILITY_LABELS.get(marker, ())]
    return ['self-hosted', 'linux', 'docker', *(capabilities or [target])]


def generate_test_matrix(groups):
    """Adapt idf-ci groups to GitHub runners and worker execution types."""
    jobs = []
    for cases in groups:
        case = cases[0]
        if len(case.targets) != 1:
            raise ValueError(f'Multi-DUT tests are not supported by the single-target worker: {case.item.nodeid}')
        target = case.targets[0]
        execution = case.emulator_marker or ('linux' if target == 'linux' else 'hardware')
        jobs.append({
            'idf_target': target,
            'marker': case.env_selector or ('host_test' if case.is_host_test else 'unmarked'),
            'execution': execution,
            'runner_labels': runner_labels(case, target, execution),
            # Newline-joined: the worker feeds this to pytest via `mapfile`,
            # which splits on newlines only, so node IDs may contain spaces.
            'nodes': '\n'.join(sorted({case.item.nodeid for case in cases})),
        })
    return {'include': jobs}


def eligible_apps(info_files):
    """Reuse the build's recorded status and manifest test-disable reason."""
    from idf_build_apps.constants import BuildStatus
    from idf_ci import PytestApp
    from idf_ci.settings import CiSettings

    # Native canonical paths identify app/target/config; no binaries are needed.
    return {
        PytestApp(app.app_dir, app.target, app.config_name).build_dir
        for app in CiSettings.read_apps_from_files(info_files) or []
        if app.build_status == BuildStatus.SUCCESS and not app.test_comment
    }


def plan_tests(info_files):
    eligible = eligible_apps(info_files)
    if not eligible:
        return {'include': []}
    from idf_ci.idf_pytest import GroupedPytestCases

    groups = GroupedPytestCases(collect_test_cases(eligible)).grouped_cases
    return generate_test_matrix(groups.values())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build_info_dir', type=Path)
    parser.add_argument('--selection-report', type=Path, default=Path('test-selection.json'))
    args = parser.parse_args()
    info_files = sorted(args.build_info_dir.glob('build_info_*.json'))
    # Never silently interpret a missing shard artifact as an empty build result.
    build_matrix = os.environ.get('BUILD_MATRIX')
    if not build_matrix:
        raise SystemExit('BUILD_MATRIX is required: pass the generate job\'s build_matrix output')
    expected = {
        f'build_info_{job["idf_target"]}_{job["parallel_index"]}.json'
        for job in json.loads(build_matrix)['include']
    }
    actual = {path.name for path in info_files}
    if actual != expected:
        raise SystemExit(f'Build metadata mismatch: missing={sorted(expected - actual)}, unexpected={sorted(actual - expected)}')
    matrix = plan_tests(info_files)
    args.selection_report.write_text(json.dumps(matrix, indent=2) + '\n')
    print(json.dumps(matrix, indent=2))
    with open(os.environ['GITHUB_OUTPUT'], 'a') as output:
        output.write(f'test_matrix={json.dumps(matrix)}\n')
        output.write(f'has_tests={str(bool(matrix["include"])).lower()}\n')


if __name__ == '__main__':
    main()
