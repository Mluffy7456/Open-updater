# OpenUpdater

Open-source, cross-platform application updater written in modern C++20.

## Status

v1.4.0 — Package signatures

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
- optional Qt 6 desktop GUI;
- stable C++20 high-level updater API;
- typed updater errors via UpdateError;
- configurable backup, rollback and verification policy;
- configurable GitHub HTTP headers;
- runtime platform and architecture detection;
- platform/architecture-aware GitHub release asset selection;
- multi-component update discovery without installation;
- core-level selection and selective installation of discovered updates;
- optional Ed25519 package signature verification backed by OpenSSL;
- asynchronous GUI operations;
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

## C++ API

New integrations should use the v1.0 high-level API:

    #include "openupdater/core/updater.hpp"

    openupdater::UpdateRequest request{
        openupdater::Version("1.2.0"),
        "owner/repository",
        "DemoApp-1.3.0.zip",
        "./updates"
    };

    const auto result = openupdater::Updater::update(request);

The request defaults to backup, automatic rollback, and digest verification when a digest is available. Operation failures are reported as `openupdater::UpdateError` with a typed `ErrorCode`.

See `docs/API.md` for the stable API contract and compatibility notes.

### Package signatures

OpenUpdater 1.4 adds detached Ed25519 signatures on top of SHA-256 verification. The signed message is the raw 32-byte SHA-256 digest of the package.

A GitHub release can publish:

    Core-windows-x64.zip
    Core-windows-x64.zip.sig

The .sig asset contains 128 hexadecimal characters (64 signature bytes). The trusted Ed25519 public key is supplied by the application and is never taken from the update server.

Signature verification is opt-in through UpdateOptions:

    openupdater::UpdateOptions options;
    options.verify_signature = true;
    options.trusted_public_key = "<64 hexadecimal characters>";

When enabled, OpenUpdater verifies the package signature before backup or installation. Missing signatures, missing trusted keys, malformed signatures, and invalid signatures are rejected.

The CLI also provides:

    openupdater verify-signature <package> <signature-hex> <public-key-hex>

### Update selection

OpenUpdater 1.3 adds selection between discovery and installation. Selection is part of the core API, so GUI, CLI and other clients can use the same policy.

    openupdater::UpdateSelection selection;
    selection.select("Core");
    selection.select("Plugins");
    selection.deselect("GUI");

    openupdater::SelectedUpdateRequest request{
        discovered_updates,
        selection,
        "./updates"
    };

    const auto reports =
        openupdater::Updater::update_selected(request);

Only components explicitly selected by the caller are downloaded and installed. Selecting a component that is not present in the discovery result is rejected. An empty selection is also rejected, preventing an accidental batch installation.

The selected-update flow keeps the existing SHA-256 verification, backup, rollback and staging policies from UpdateOptions.

### Multi-platform assets

OpenUpdater 1.1 can detect the host platform and CPU architecture. Compatible GitHub assets use the naming convention `<component>-<platform>-<architecture>.<extension>`.

Examples: `DemoApp-windows-x64.zip`, `DemoApp-linux-x64.tar.gz`, `DemoApp-linux-arm64.tar.gz`, `DemoApp-macos-arm64.zip`.

Use `GitHubReleasesProvider::latest_compatible(...)` to select the compatible asset. If multiple compatible assets exist for the same component/platform/architecture, selection fails instead of guessing.

### Update discovery

OpenUpdater 1.2 separates **checking** from **installing**. The core API can check several components in one request and returns only components for which a compatible asset exists and the release version is newer than the installed version.

    openupdater::UpdateDiscoveryRequest request{
        "owner/repository",
        {
            {"Core", openupdater::Version("1.4.0")},
            {"GUI", openupdater::Version("1.2.0")}
        }
    };

    const auto updates =
        openupdater::Updater::discover_updates(request);

Each `AvailableUpdate` contains the component, current and available versions, selected asset, platform, architecture, SHA-256 digest when supplied by GitHub, and download URL.

Nothing is downloaded or installed during discovery. This API is the foundation for the next selection stage, where the caller will choose which discovered components to install.

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
- 1.1.0 — Multi-platform asset detection and selection
- 1.2.0 — Update discovery
- 1.3.0 — Update selection and selective installation
- 1.4.0 — Ed25519 package signature verification
- 1.4.0 — Digital package signatures
- 2.0.0 — Full update manager

## License

MIT.
