# Changelog

## [2.0.0] - 2026-10-01

### Added
- Dependency-aware multi-component update planning.
- Component metadata with minimum dependency versions.
- Extended manifests with [component:<name>] sections.
- Topological installation order with dependencies before dependents.
- Automatic inclusion of required dependency updates.
- Dependency cycle and unsatisfied dependency detection.
- Transactional update execution with complete preflight download and verification.
- Transaction-level rollback of already-installed components.
- New DependencyFailed and TransactionFailed error codes.
- UpdatePlan, UpdateManagerRequest and UpdateTransactionReport APIs.
- Manager and manifest unit tests.
- v2.0 component manifest example.

### Changed
- Public API and project version updated to 2.0.0.
- Transactional updates require backup and automatic rollback to avoid silent partial state.
- Client version identifiers and GitHub User-Agent updated to OpenUpdater/2.0.

## [1.4.0] - 2026-10-01

### Added
- Ed25519 detached package signature verification.
- SHA-256 digest based signature verification using OpenSSL.
- GitHub discovery of matching <asset>.sig release assets.
- Signature policy in UpdateOptions.
- CLI verify-signature command.
- Signature verification tests using an Ed25519 test vector.

### Changed
- Public API and project version updated to 1.4.0.
- CI now installs OpenSSL on Linux and Windows.

## [1.3.0] - 2026-10-01

### Added
- Core-level update selection API.
- UpdateSelection with select, deselect, clear and select-all operations.
- SelectedUpdateRequest for selective installation.
- Updater::update_selected(...) for installing only explicitly selected discovered updates.
- Tests for selection behavior and validation.

### Changed
- Public API and project version updated to 1.3.0.

## [1.2.0] - 2026-10-01

### Added
- Multi-component update discovery API.
- UpdateTarget, AvailableUpdate and UpdateDiscoveryRequest types.
- Updater::discover_updates(...) for checking updates without downloading or installing packages.
- Single GitHub metadata request for discovery across multiple components.

### Changed
- Public API and project version updated to 1.2.0.
- GitHub provider User-Agent updated to OpenUpdater/1.2.

## [1.1.0] - 2026-10-01

### Added
- Runtime platform detection for Windows, Linux and macOS.
- Runtime architecture detection for x64, x86, ARM64 and ARM32.
- Platform and architecture naming helpers.
- Platform-aware GitHub release asset matching.
- GitHubReleasesProvider::latest_compatible(...) for automatic compatible asset selection.
- Unit coverage for platform detection and asset matching.

### Changed
- Project and public API version updated to 1.1.0.
- GitHub provider User-Agent updated to OpenUpdater/1.1.

## [1.0.0] - 2026-10-01

### Added
- Stable high-level C++20 updater API.
- Versioned API constants for 1.0.0.
- Typed UpdateError and ErrorCode failure model.
- UpdateRequest, UpdateOptions and UpdateReport.
- Configurable backup, automatic rollback and download verification policy.
- Configurable GitHub HTTP headers.
- Stable API reference in docs/API.md.
- CLI update flow routed through the stable API.
- Stable API unit coverage.

### Changed
- Project version updated to 1.0.0.
- CLI and GUI version labels updated to 1.0.0.
- Windows WinHTTP User-Agent updated to OpenUpdater/1.0.0.

## [0.6.0] - 2026-10-01

### Added
- Optional Qt 6 desktop GUI.
- GUI fields for current version, repository, release asset, destination and SHA-256.
- Asynchronous update checking.
- Asynchronous update installation.
- GUI display of generated backup paths.
- CMake option OPENUPDATER_BUILD_GUI.

### Changed
- Project version updated to 0.6.0.

## [0.5.0] - 2026-10-01

### Added
- Unattended update flow from the latest GitHub release.
- Silent CLI mode with --silent.
- Automation-friendly exit codes for the update command.
- Update flow that checks the version before downloading the package.

### Changed
- Windows WinHTTP User-Agent updated to OpenUpdater 0.5.0.

## [0.4.0] - 2026-10-01

### Added
- Backup manager for installed packages.
- Staged installation through a temporary file.
- Automatic backup before replacing an existing package.
- Automatic rollback attempt when activation fails.
- CLI rollback command.
- Backup and rollback unit tests.

### Changed
- Updater::install now returns the created backup path when an existing package was replaced.
- Windows WinHTTP User-Agent updated to OpenUpdater 0.4.0.

## [0.3.0] - 2026-10-01

### Added
- GitHub Releases provider for latest release assets.
- GitHub asset lookup by exact filename.
- Optional GitHub release asset SHA-256 digest verification.
- CLI github command.
- HTTP request headers for the downloader.

## [0.2.1] - 2026-10-01

### Added
- Portable SHA-256 implementation.
- File hash verification.
- CLI verify command.
- Optional SHA-256 verification during installation.

## [0.2.0] - 2026-10-01

### Added
- Cross-platform HTTP/HTTPS downloader.
- Native WinHTTP implementation on Windows.
- libcurl implementation on non-Windows platforms.
- CLI download command.

## [0.1.0] - 2026-10-01

### Added
- Semantic numeric version comparison.
- Key/value update manifest parser.
- Update availability check.
- Local package installation.
- CLI foundation.
- CMake build.
- Ubuntu and Windows CI.
