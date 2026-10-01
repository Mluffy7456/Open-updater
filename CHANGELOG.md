# Changelog

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

# Changelog

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
