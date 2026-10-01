# Changelog

## [0.2.1] - 2026-10-01

### Added
- Portable SHA-256 file hashing without external cryptography dependencies.
- SHA-256 known-answer tests for empty and non-empty files.
- Package verification before installation.
- CLI `verify` command.
- Optional SHA-256 verification for the `install` command.

## [0.2.0] - 2026-10-01

### Added
- Cross-platform HTTP/HTTPS downloader.
- Native Windows WinHTTP implementation.
- Linux libcurl implementation.
- CLI download command.
- Automatic creation of the download destination parent directory.

## [0.1.0] - 2026-10-01

### Added
- C++20 core library.
- Numeric version comparison with optional v-prefix.
- Simple key/value update manifest.
- Update availability checks.
- Local package installation.
- CLI commands: version, check, install.
- Cross-platform CMake build.
- Ubuntu and Windows CI.
- Core unit tests.

### Notes
- Package hash/signature verification is planned for v0.2.1.
