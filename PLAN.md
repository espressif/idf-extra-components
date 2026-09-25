# Plan: Migrate CI to idf-ci (GitHub equivalent of the GitLab child pipelines)

## Goal

Replace the custom glue (`get_idf_build_apps_args.py`, `get_pytest_args.py`,
`generate_build_matrix.py`) and raw `idf-build-apps` invocations with `idf-ci`,
mirroring the esp-idf GitLab pipeline structure
(`common/templates/idf/build.yml`): a generator job computes what to build and
test, downstream jobs run from dynamic matrices.

## Current state (as-is)

- The project caller owns triggers, labels, changed root-level components,
  reporting and the outer IDF-version matrix in `.github/ci-matrix.json`.
- Public `reusable-ci.yml` accepts one `idf_version`, project config, a test
  profile, a runtime-test switch and structured change-selection data.
- Every invocation collects in its own IDF container. Its target workers
  build and test independently, including Linux; remaining targets get
  compile coverage. Empty matrices are skipped explicitly.
- Runner/shard policy lives in `.github/ci-config.json`. Generic named test
  profiles replace the separate per-marker lists of IDF versions.
- Shared scripts and tool pins are packaged in `.github/actions/ci-tools`.
  Self-repository action references (`$/`) keep them on the CI revision
  when another repository invokes the workflow.
- Builds still use `idf-build-apps`. Metadata drives tar artifact packaging
  and the existing pytest app selection; paths and Linux executable modes
  survive upload/download independently of the consumer's directory layout.
- Discovery counts the full app inventory for the selected IDF. Existing
  component-dependency filtering is retained at build time.

## Target state (to-be)

- `prepare`: label handling only; exposes `changed_files.txt` as an artifact.
- `generate`: `idf-ci` computes affected apps (modified-files filtering,
  manifest-based — no reconfigure storm), splits test-related /
  non-test-related, sizes shards by real app counts, and emits build and test
  matrices (`idf-ci test collect --format github`).
- Build jobs: `idf-ci build run` (`--only-test-related` /
  `--only-non-test-related`).
- Test jobs: matrix entries come from collected pytest cases; no
  `get_pytest_args.py`.

## Steps

### 1. PoC: validate idf-ci on this repo — DONE (2026-09-24)

Run locally against a master-ish IDF (6.2) with `idf-ci 1.3.0` +
`idf-build-apps 2.16.1`:

- `idf-ci build collect -p . --format json` works: exit 0, ~3 min including
  full pytest collection. Result: 84 projects, 443 apps "should be built",
  671 disabled by manifest rules, 183 test cases collected (this also
  confirms the `target` parametrize fixes landed — the cases are visible).
- The extra targets `esp32s31`/`esp32h21` in the output come from the local
  IDF fork's preview targets, not from this repo.
- Version pin: idf-ci 1.3.0 requires `idf-build-apps>=2.16.1,<4`; our
  `~=2.12` pin resolves to 2.16.1, so they are compatible. Consider
  tightening the pin to `~=2.16`.

**Collection issue found:** test-case↔app join is broken for apps without an
`sdkconfig.ci*` file. idf-ci builds its app key with the raw
`config_name` (`''` for default builds, see `build_collect/scripts.py:73`),
while pytest cases default to `config='default'`
(`idf_pytest/models.py:58`). Result: 149 of 183 test cases land in
`missing_apps`. 100% correlation confirmed: every "used" case belongs to an
app with an `sdkconfig.ci*` file, every app without one is "missing".
Options:
  a. Fix upstream in idf-ci (normalize `app.config_name or 'default'` when
     building the AppKey) — the right fix, esp-idf CI is unaffected because
     its test apps all carry `sdkconfig.ci*` files;
  b. Workaround: add empty `sdkconfig.ci` to every test app (dozens of
     files, ugly);
  c. Workaround: parametrize `config` in every pytest file (ugly);
  d. Add an `=default` fallback to `config_rules` in `.idf_build_apps.toml`.
     This names previously unnamed builds `default` and changes their build
     directory to `build_<target>_default`; named configurations are unchanged.
     Validate the artifact paths and one real build before adopting it.

**Scope clarified (2026-09-25):** published idf-ci 1.3.0's `build run`
normalizes `app.config_name or 'default'` in `get_all_apps`, so this report
issue does not itself block building. However, its native pytest artifact
filter expects `build_<target>_default` for these cases, while the current
unnamed builds use `build_<target>`. Do not replace `get_pytest_args.py`
with native filtering until these paths agree.

### 2. Rework the `generate` job onto idf-ci — DONE (2026-09-24)

- `.github/generate_build_matrix.py` now shells out to idf-ci:
  `build collect` for per-target buildable-app counts (build_status ==
  "should be built" — the previous find_apps version also counted
  manifest-disabled apps, oversizing shards ~2.4x) and `test collect
  --format github` (twice: `-m 'not host_test'` and `-m qemu`, since idf-ci
  treats emulator-marked cases as host tests) for the per-target test
  markers actually present.
- `pytest.ini` gained an `env_markers` section — the idf-ci pytest plugin
  reads it to group cases per runner environment.
- ci-matrix.json `test_configs` remains the runner-availability policy;
  the generator intersects it with collected markers (e.g. the quad_psram
  case is collected but not scheduled — no such runner).
- Ethernet tests (`coap_client`, `sh2lib`) were parametrized to esp32 only —
  the ethernet runner is an ESP32-ETHERNET-KIT.
- Measured locally (IDF 6.2): esp32 56 apps -> 2 shards, esp32s2 32 -> 1,
  esp32s3 44 -> 2, esp32c3 47 -> 2 (was: fixed 5 everywhere), other targets
  203 -> 5 shards (cap). esp32c3 correctly keeps only its qemu config.

### 2a. Stabilize the reusable workflow contract — DONE (2026-09-25, historical)

- Apply `idf_versions_target_test` to generic/QEMU/SPI NAND tests and
  `idf_versions_ethernet_test` independently to Ethernet tests.
- Treat latest-only discovery as authoritative only for `latest`. For older
  IDF versions keep configured runners and at least one build shard, even if
  latest reports no apps or tests. Older-version manifests still decide what
  actually builds. This may schedule empty jobs but cannot drop historical
  coverage based on latest's manifest rules.
- Expose `has_apps`/`has_extra` and skip empty matrices before expansion;
  skip expensive generation when the prepare job says there is no component
  work. The test-script check and matrix unit tests still run.
- Register the CoAP manifest in `.idf_build_apps.toml`; otherwise the
  ESP32-only Ethernet rule is ignored and the test-script check fails.
- Linux artifact upload now follows the same build-only condition as Linux
  test execution for PR, push and schedule events. The shared setup action
  probes the host compiler for Linux warning flags.
- Keep PR test publishing in the job summary for read-only fork/Dependabot
  tokens; same-repository PRs retain checks and comments.
- Add stdlib unit tests for matrix policies, historical coverage and shards,
  and document the current pipeline in `.github/readme_workflows.md`.

### 2b. One reusable invocation per IDF version — DONE (2026-09-25)

- Move the version matrix to the project caller. Add public
  `.github/workflows/reusable-ci.yml` for exactly one IDF environment.
- Move discovery inside that workflow and remove all `latest` extrapolation
  and conservative historical fallback logic introduced in step 2a.
- Replace version-dependent marker branches with project-owned named test
  profiles. Preserve the existing Linux test exclusions for 5.2 and 6.0.
- Use the same internal target worker for ESP and Linux. Pass change lists
  as JSON data rather than interpolated `idf-build-apps` command fragments.
- Package shared scripts as an action resolved from the CI workflow's own
  commit. Remove the old reusable matrix reader and repo-root helper paths.
- Archive runtime files from build metadata instead of hardcoded
  `examples/test_apps` globs, preserving layout and executable permissions.
- Keep reporting and the stable gate in the outer caller; artifact names
  isolate each version, target and shard.
- Local validation: unit coverage for independent environments, profiles,
  empty discovery, sharding, archive layout/modes and structured arguments;
  real collection with published idf-ci 1.3.0/idf-build-apps 2.16.1 against
  SDK 5.2.6 and the current local SDK. Actual GitHub and hardware runs remain
  integration validation, including cross-repository action resolution.

### 3. Switch build steps to `idf-ci build run`

Prerequisites confirmed against published idf-ci 1.3.0:

- Its CLI has no `--modified-components`, `--collect-app-info` or
  `--disable-targets`; the current argument string cannot be passed through.
- Keep the build-info contract using idf-build-apps configuration (for example
  `collect_app_info_filename = 'build_info_@p.json'`) before replacing the
  consumer. Audit app/config path matching, including unnamed defaults.
- The default modified-component mapping assumes `components/` or
  `common_components/`; this repository stores components at its root.
  Complete step 6's dependency audit before changing affected-app selection.
- Include QEMU deliberately when splitting test-related builds: idf-ci's
  default collection excludes emulator cases, while an explicit nonempty
  marker expression forces test-related-only builds.

- In `reusable-build-run-apps.yml` and `build-extra`: replace
  `idf-build-apps build ...` with `idf-ci build run -t <target>
  --parallel-index N --parallel-count M [--modified-files ...]
  [--only-test-related|--only-non-test-related]`.
- Verify artifact contract: build dirs layout (`build_<target>[_<config>]`),
  binaries, `flasher_args.json`, and what replaces `build_info*.json` for the
  test side (idf-ci re-collects cases in the test job instead — see step 4).
- Confirm `EXTRA_CFLAGS`/`EXTRA_CXXFLAGS` pedantic flags are honored
  (idf-ci shells out to idf.py; env-based flags should still apply — verify
  on one component).

### 4. Switch test selection to `idf-ci test collect`

- Generate the test matrix from collected cases (grouped by target + env
  marker) instead of static `test_configs` in ci-matrix.json.
- Test job runs pytest with the collected case list / `-m` marker directly;
  delete the packaged `actions/ci-tools/get_pytest_args.py`.
- Keep the runner-label mapping (generic/ethernet/spi_nand_flash/qemu)
  somewhere explicit — either in ci-matrix.json or derived from markers.

### 5. Clean up the prepare job and delete custom scripts

- `prepare` keeps only label handling and produces `changed_files.txt`.
- The shell-argument helper was replaced in step 2b by caller-owned
  `get_ci_changes.py`; migrating selection to manifests remains pending.

### 6. Resolve modified-files vs modified-components semantics

- Current flow filters by *component dependencies* (cmake reconfigure per
  app); idf-ci filters by *manifest filepatterns* (`.build-test-rules.yml`).
- Audit each component's `.build-test-rules.yml` for `depends_filepatterns`
  coverage; add missing rules, otherwise affected apps may not rebuild.
- Decide whether to keep component-level filtering anywhere (idf-build-apps
  supports it, idf-ci's GitLab flow relies on manifests).

### 7. Migrate Linux execution commands

- Linux already shares the target worker after step 2b. Verify
  `idf-ci build run -t linux` and host_test artifact selection when replacing
  its execution commands; preserve the caller-selected test profile.

### 8. Validation

- PR with `PR: test all apps`: diff the app list old flow vs new flow
  (`idf-build-apps find` vs `idf-ci build collect`) — must match.
- CI-only PR → generator yields empty matrices, everything skipped.
- Full scheduled run green on all 6 IDF versions, tests run on all runners
  (esp32/s2/s3 hardware, ethernet, spi_nand_flash, qemu s3/c3, linux).

### 9. Docs

- Update workflow header comments, README/AGENTS.md if they describe the CI
  flow.

## Risks / open questions

- **idf-ci maturity on GitHub**: the GitLab path is the tested one; the
  GitHub building blocks (`test collect --format github`) may have gaps —
  validate in step 1 before rewriting jobs.
- **pytest collection cost**: the generator will collect all pytest cases
  (~1–2 min) — acceptable, but a broken test file import will fail the whole
  pipeline early (arguably a feature).
- **Manifest-rule coverage** (step 6) is the main correctness risk:
  incomplete `depends_filepatterns` = missed rebuilds.
- **Version coverage**: discovery now runs in every selected IDF environment.
  Run the full GitHub matrix to validate tool installation and hardware tests
  across all supported images, not only local SDK source checkouts.
- **Pinned versions**: keep `idf-ci` and `idf-build-apps` pins compatible;
  re-check on every bump.
