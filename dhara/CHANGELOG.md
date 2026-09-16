## [1.0.1]

### Fixes / hardening (Espressif patches on vendored baseline)

- Fix `dhara_journal_peek()` returning a stale bad-block `tail` when retries are exhausted; return `DHARA_PAGE_NONE` instead.
- Fix unsigned underflow / out-of-bounds `memset` in `hdr_clear_user()` when the page is smaller than header+cookie.
- Make `recover_from()`'s fast path also require checkpoint alignment before accepting the recovered head.
- Avoid negative-shift undefined behaviour in `dhara_journal_capacity()` when `log2_ppc > log2_ppb`.
- Make `dhara_map_gc()` perform at most one GC step per call.
- Clamp `dhara_map_size()` to `dhara_map_capacity()`.
- Make `dhara_journal_root()` defensively handle an empty journal.
- Add a compile-time guard against `dhara_sector_t` truncation in `ck_set_count()`.

### Docs

- Clarify intentional contracts / edge cases in journal and map helpers (comments only).
- Document that `find_head()` cannot fail (resume's `< 0` check is always false).
- Document why the two `memcpy` of `DHARA_META_SIZE` in `journal.c` are in-bounds.

### API

- `dhara_journal_read_meta()` now takes `const struct dhara_journal *` (source-compatible).

## [1.0.0]

### Versioning

This release starts a **separate semver line for this ESP-IDF component** (`dhara` in idf-extra-components), published via `idf_component.yml`. The FTL sources still come from upstream [dlbeer/dhara](https://github.com/dlbeer/dhara), vendored at a pinned baseline ([VENDORED_UPSTREAM.md](VENDORED_UPSTREAM.md)); upstream’s own tags are **not** the version consumers should pin for this component.

From **1.0.0** onward we intend to align this component’s **MAJOR.MINOR.PATCH** bumps with [Semantic Versioning 2.0.0](https://semver.org/spec/v2.0.0.html) at this packaging boundary (vendored code, Espressif patches, build or public API surface exposed through this repo). Notable changes will be listed in this file per release.

### Packaging

- Replace the **git submodule** with an in-tree **vendored** snapshot under `dhara/dhara/` (ordinary tracked files; no submodule checkout required).
- Update **SBOM** and component metadata for the vendored tree (`sbom_dhara.yml`, `idf_component.yml`).

### Behavior

- **No intentional FTL behavior change** relative to the previous submodule layout at the same upstream baseline.
