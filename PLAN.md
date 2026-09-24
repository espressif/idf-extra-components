# Plan: Migrate CI to idf-ci (GitHub equivalent of the GitLab child pipelines)

## Goal

Replace the custom glue (`get_idf_build_apps_args.py`, `get_pytest_args.py`,
`generate_build_matrix.py`) and raw `idf-build-apps` invocations with `idf-ci`,
mirroring the esp-idf GitLab pipeline structure
(`common/templates/idf/build.yml`): a generator job computes what to build and
test, downstream jobs run from dynamic matrices.

## Current state (as-is)

- `prepare` job: label handling + `get_idf_build_apps_args.py` →
  `--modified-files` / `--modified-components` args (component-level filtering
  requires a cmake reconfigure per app at build time).
- `generate` job: `generate_build_matrix.py` counts apps via
  `idf_build_apps.find_apps` and emits shard matrices (no test awareness, no
  modified-files filtering).
- `apps` / `build-extra` / `build-linux`: build with `idf-build-apps build`
  and upload binaries + `build_info*.json`.
- Test jobs: select cases via `get_pytest_args.py` (reads `build_info*.json`,
  `--ignore`s apps not built for the target) + pytest with `-m <marker>`.

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

### 1. PoC: validate idf-ci on this repo

- Run inside `espressif/idf:latest` container:
  `idf-ci init` (creates `.idf_ci.toml`), review `settings.exclude_dirs`,
  `preserve_*` options.
- `idf-ci build collect -p . --format json -o collect.json` — check the app
  list matches `idf-build-apps find` output (same app/target/config set).
- `idf-ci test collect --format github` — verify pytest collection succeeds
  for all `pytest_*.py` (needs pytest, pytest-embedded, pytest-embedded-idf,
  pytest-embedded-qemu installed in the container).
- Pin the idf-ci version used in CI (check which idf-build-apps version it
  pulls; align with the current `idf-build-apps~=2.12` pin).

### 2. Rework the `generate` job onto idf-ci

- Inputs: `changed_files.txt` artifact from `prepare` (PRs), ci-matrix.json.
- Use `idf_ci.scripts.get_all_apps(modified_files=..., marker_expr='not
  host_test')` → (test_related, non_test_related); shard counts via the same
  `ceil(apps / runs_per_job)` rule.
- Emit `apps_matrix` / `extra_matrix` as today, but sized by *affected* apps;
  empty result → empty include matrix (job skipped) — this replaces the
  `has_changes` gate.
- Delete `.github/generate_build_matrix.py` when done.

### 3. Switch build steps to `idf-ci build run`

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
  delete `.github/get_pytest_args.py`.
- Keep the runner-label mapping (generic/ethernet/spi_nand_flash/qemu)
  somewhere explicit — either in ci-matrix.json or derived from markers.

### 5. Clean up the prepare job and delete custom scripts

- `prepare` keeps only label handling and produces `changed_files.txt`.
- Delete `.github/get_idf_build_apps_args.py`.

### 6. Resolve modified-files vs modified-components semantics

- Current flow filters by *component dependencies* (cmake reconfigure per
  app); idf-ci filters by *manifest filepatterns* (`.build-test-rules.yml`).
- Audit each component's `.build-test-rules.yml` for `depends_filepatterns`
  coverage; add missing rules, otherwise affected apps may not rebuild.
- Decide whether to keep component-level filtering anywhere (idf-build-apps
  supports it, idf-ci's GitLab flow relies on manifests).

### 7. Migrate the Linux pipeline

- Verify `idf-ci build run -t linux` and host_test marker handling; migrate
  `build-linux` / `run-target-linux` the same way, keeping the
  `idf_versions_linux_test` subset.

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
- **Version skew**: generator runs on `latest`; per-version manifest/target
  differences remain approximate (shard sizing only).
- **Pinned versions**: keep `idf-ci` and `idf-build-apps` pins compatible;
  re-check on every bump.
