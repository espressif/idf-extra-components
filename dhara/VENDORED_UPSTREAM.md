# Dhara upstream baseline

This directory is vendored from **https://github.com/dlbeer/dhara**.

- **Baseline commit:** `1b166e41b74b4a62ee6001ba5fab7a8805e80ea2`
- **Vendored on:** `2026-04-14`
- **Policy:** Espressif may apply patches here. For upstream bugfixes/features, prefer
  cherry-picking or re-baselining to a newer upstream commit, then update this file and
  `sbom_dhara.yml`.

## Refresh procedure (maintainers)

1. Compare this tree to upstream: `git fetch` in a separate clone of dlbeer/dhara, diff against `1b166e41b74b4a62ee6001ba5fab7a8805e80ea2`.
2. Merge or cherry-pick desired commits.
3. Update **Baseline commit** above and `sbom_dhara.yml` (`version` / `hash` per org rules).
4. Bump `dhara/idf_component.yml` version and document user-visible FTL changes in changelogs.
