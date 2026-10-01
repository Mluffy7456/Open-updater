# OpenUpdater 1.0 API

OpenUpdater 1.0 exposes a stable C++20 core API. The GUI and CLI are clients of the same core library.

## Version

Use the compile-time API version constants:

    openupdater::API_VERSION_MAJOR
    openupdater::API_VERSION_MINOR
    openupdater::API_VERSION_PATCH
    openupdater::API_VERSION

The 1.0 API is intended to remain source-compatible across 1.x releases unless a documented deprecation is introduced.

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

The following existing APIs remain available in 1.0:

- Version
- Manifest and load_manifest
- Downloader
- GitHubReleasesProvider
- BackupManager
- Updater::check
- Updater::install
- Updater::update_from_github

The high-level Updater::update should be preferred for new integrations.
