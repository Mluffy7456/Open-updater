# OpenUpdater 1.2 API

OpenUpdater 1.0 exposes a stable C++20 core API. The GUI and CLI are clients of the same core library.

## Version

Use the compile-time API version constants:

    openupdater::API_VERSION_MAJOR
    openupdater::API_VERSION_MINOR
    openupdater::API_VERSION_PATCH
    openupdater::API_VERSION

The 1.2 API is intended to remain source-compatible across 1.x releases unless a documented deprecation is introduced.

## Platform and architecture

The core exposes `Platform` and `Architecture` enums plus `current_platform()` and `current_architecture()` for runtime host detection.

`platform_name()` and `architecture_name()` return stable lowercase identifiers such as `windows`, `linux`, `macos`, `x64` and `arm64`.

`GitHubReleasesProvider::latest_compatible(...)` selects an asset using the convention `<component>-<platform>-<architecture>.<extension>`. It rejects unknown host information and ambiguous multiple matches rather than silently selecting an arbitrary package.

## Update discovery

The high-level discovery entry point is:

    openupdater::Updater::discover_updates(
        const openupdater::UpdateDiscoveryRequest&)

UpdateDiscoveryRequest contains:

- repository — GitHub owner/repository;
- targets — components and their installed versions;
- platform — target platform, defaulting to the current host;
- architecture — target CPU architecture, defaulting to the current host;
- headers — optional HTTP headers.

For every target, discovery looks for a compatible asset in the latest published GitHub release. It returns only targets whose release version is newer than the supplied current version. A missing compatible asset is omitted from the result.

AvailableUpdate contains:

- component;
- current;
- available;
- asset;
- platform;
- architecture;
- sha256;
- download_url.

Discovery performs only metadata retrieval. It does not download packages, create backups, or install anything. The returned vector is intended for a later selection/installation stage.

## Update request

The high-level entry point is:

    openupdater::Updater::update(const openupdater::UpdateRequest&)

A request contains:

- current — installed application version;
- repository — GitHub owner/repository;
- asset_name — exact release asset filename;
- destination — installation directory;
- expected_sha256 — optional caller-supplied digest;
- options — backup, rollback and verification policy.

UpdateOptions defaults to the conservative behavior:

- create a backup when an existing package is replaced;
- attempt automatic rollback when activation fails;
- verify a package when a digest is available;
- use no additional HTTP headers.

## Result

UpdateReport contains:

- current — version supplied by the caller;
- available — latest GitHub release version;
- state — UpToDate or UpdateAvailable;
- package — installed package path when an update was installed;
- backup — backup path when an existing package was replaced.

When state == UpdateState::UpToDate, no package is downloaded.

## Errors

The stable API throws openupdater::UpdateError.

Inspect error.code() and handle:

- InvalidArgument
- InvalidVersion
- UpdateCheckFailed
- DownloadFailed
- VerificationFailed
- InstallationFailed
- RollbackFailed

The legacy lower-level functions remain available for applications that need direct control over individual operations.

## Compatibility

The following existing APIs remain available in 1.x:

- Version
- Manifest and load_manifest
- Downloader
- GitHubReleasesProvider
- BackupManager
- Updater::check
- Updater::install
- Updater::update_from_github

The high-level Updater::update should be preferred for direct single-package updates. For workflows that inspect multiple components before installing, use Updater::discover_updates first.

The exact-asset GitHub API remains available for applications that need explicit asset selection.
