## [1.0.0]

### Versioning

This release starts a **separate semver line for this ESP-IDF component** (`dhara` in idf-extra-components), published via `idf_component.yml`. The FTL sources still come from upstream [dlbeer/dhara](https://github.com/dlbeer/dhara), vendored at a pinned baseline ([VENDORED_UPSTREAM.md](VENDORED_UPSTREAM.md)); upstream’s own tags are **not** the version consumers should pin for this component.

From **1.0.0** onward we intend to align this component’s **MAJOR.MINOR.PATCH** bumps with [Semantic Versioning 2.0.0](https://semver.org/spec/v2.0.0.html) at this packaging boundary (vendored code, Espressif patches, build or public API surface exposed through this repo). Notable changes will be listed in this file per release.

### Packaging

- Replace the **git submodule** with an in-tree **vendored** snapshot under `dhara/dhara/` (ordinary tracked files; no submodule checkout required).
- Update **SBOM** and component metadata for the vendored tree (`sbom_dhara.yml`, `idf_component.yml`).

### Behavior

- **No intentional FTL behavior change** relative to the previous submodule layout at the same upstream baseline.
