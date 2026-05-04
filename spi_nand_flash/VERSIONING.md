# Versioning

From **v1.0.0** onward, this component follows **[Semantic Versioning 2.0.0](https://semver.org/spec/v2.0.0.html)**. The published version is the `version` field in [`idf_component.yml`](idf_component.yml).

## Summary

| Level   | When to bump |
|--------|----------------|
| **MAJOR** | Breaking changes for integrators: removed or incompatible APIs, required migration, or other incompatible contract changes called out under **Breaking Changes** in [`CHANGELOG.md`](CHANGELOG.md). |
| **MINOR** | Backward-compatible additions: new APIs, new supported devices or modes, new optional Kconfig (default preserves prior behavior). |
| **PATCH** | Backward-compatible fixes only: bug fixes, documentation corrections, tests, or build/CI changes that do not widen the public API or break existing callers. |

Pre-**1.0.0** releases (`0.x.y` in the changelog) did not consistently apply this split; treat them as historical versioning. Use **1.x.y** and the rules above for current releases.
