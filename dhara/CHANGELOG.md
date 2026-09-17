# Changelog

All notable changes to this component will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this component adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.1.0] - 2026-09-18

### Added

- Sequential-locality path cache in the Dhara FTL map (`trace_path`), enabled
  by default. Caches the last successful read-only path traversal so
  consecutive sector lookups sharing a common radix-tree prefix skip
  already-known levels, reducing `dhara_journal_read_meta` calls for
  sequential sector access patterns.

## [1.0.0] - 2026-04-14

This release starts a **separate semver line for this ESP-IDF component**
(`dhara` in idf-extra-components), published via `idf_component.yml`. The FTL
sources still come from upstream [dlbeer/dhara](https://github.com/dlbeer/dhara),
vendored at a pinned baseline ([VENDORED_UPSTREAM.md](VENDORED_UPSTREAM.md));
upstream's own tags are **not** the version consumers should pin for this
component.

From **1.0.0** onward this component's **MAJOR.MINOR.PATCH** bumps align with
[Semantic Versioning 2.0.0](https://semver.org/spec/v2.0.0.html) at this
packaging boundary (vendored code, Espressif patches, build or public API
surface exposed through this repo).

### Changed

- Replaced the **git submodule** with an in-tree **vendored** snapshot under
  `dhara/dhara/` (ordinary tracked files; no submodule checkout required).
- Updated **SBOM** and component metadata for the vendored tree
  (`sbom_dhara.yml`, `idf_component.yml`).

No intentional FTL behavior change relative to the previous submodule layout
at the same upstream baseline.
