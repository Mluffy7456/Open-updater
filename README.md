# OpenUpdater

Open-source, cross-platform application updater written in modern C++20.

## Status

v0.5.0 — Silent Update

Features:
- semantic numeric version comparison;
- update manifest parsing;
- update availability checks;
- HTTP/HTTPS file downloading;
- SHA-256 package hashing and verification;
- GitHub Releases provider for latest release assets;
- staged package installation;
- automatic backup of an existing installed package;
- rollback of a previous installation;
- unattended update flow from GitHub Releases;
- silent CLI mode for automation/background execution;
- local package installation;
- command-line interface;
- CMake build;
- Ubuntu and Windows CI.

Windows uses the native WinHTTP API. Linux uses libcurl.

## Build

Requirements:
- C++20 compiler
- CMake 3.20+
- Git
- libcurl development package on Linux

Build:

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release
    ctest --test-dir build -C Release --output-on-failure

## CLI

Check a version:

    openupdater version 1.2.3

Check for an update:

    openupdater check 1.2.0 examples/manifest.ff

Download an update:

    openupdater download https://example.com/DemoApp-1.4.0.zip ./DemoApp-1.4.0.zip

Install a local package:

    openupdater install ./DemoApp-1.4.0.zip ./updates

If ./updates/DemoApp-1.4.0.zip already exists, OpenUpdater creates a backup under ./updates/.openupdater/backups/ before activating the new package. Installation is staged first, so a failed activation attempts to restore the previous package automatically.

The install command prints the backup path when a previous package was replaced. Restore it with:

    openupdater rollback ./updates/.openupdater/backups/DemoApp-1.4.0.zip.<timestamp>.bak ./updates/DemoApp-1.4.0.zip

Install and verify a package:

    openupdater install ./DemoApp-1.4.0.zip ./updates e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855

Verify a package without installing it:

    openupdater verify ./DemoApp-1.4.0.zip e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855

Download an asset from the latest public GitHub release:

    openupdater github owner/repository DemoApp-1.4.0.zip ./DemoApp-1.4.0.zip

The provider queries GitHub's latest published release, selects the exact asset name, downloads its browser URL, and verifies the SHA-256 digest when GitHub provides one. An explicit digest can also be supplied as the fifth argument.

Run an unattended update from the latest GitHub release:

    openupdater update 1.2.0 owner/repository DemoApp-1.4.0.zip ./updates

For background/automation use, suppress normal status output:

    openupdater update --silent 1.2.0 owner/repository DemoApp-1.4.0.zip ./updates

The update command first checks the latest published release version. If it is newer than the current version, the asset is downloaded into OpenUpdater staging, verified, and installed through the v0.4.0 backup/rollback flow. If no update is needed, nothing is downloaded. The command is non-interactive and never asks for confirmation.

Exit codes for update:
- 0 — update installed;
- 10 — no update required;
- 1 — invalid CLI arguments;
- 2 — update error.

The latest-release endpoint used by the GitHub provider represents the most recent published non-prerelease, non-draft release.

## Manifest

Example:

    application=DemoApp
    version=1.4.0
    package=DemoApp-1.4.0.zip

## Roadmap

- 0.1.0 — Foundation
- 0.2.0 — HTTP/HTTPS downloader
- 0.2.1 — SHA-256 package verification
- 0.3.0 — GitHub Releases provider
- 0.4.0 — Backup and rollback
- 0.5.0 — Silent/background update
- 0.6.0 — GUI
- 1.0.0 — Stable updater API

## License

MIT.
