# Changelog

All notable changes to this component will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this component adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.1.1] - 2026-11-12

### Fixed

- Local patch to upstream `journal.c`: `push_meta()` now retries a failed
  metadata page write in place instead of leaving the journal head on an
  already-programmed user page, which caused that page to be programmed a
  second time on the next write. Errors other than `DHARA_E_BAD_BLOCK` are
  retried up to `DHARA_MAX_RETRIES` times, then handled as a bad block.
  Retiring a possibly good block is preferred over programming a page twice.
  Relies on the `spi_nand_flash` glue reporting `DHARA_E_BAD_BLOCK` only when
  the page may have been programmed.

## [1.1.0] - 2026-09-22

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
