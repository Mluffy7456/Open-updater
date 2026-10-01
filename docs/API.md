# OpenUpdater 2.0 API

OpenUpdater 2.0 keeps the C++20 core, CLI and optional Qt 6 GUI while adding a dependency-aware update manager and atomic multi-component transactions.

## Version

Use:

    openupdater::API_VERSION_MAJOR
    openupdater::API_VERSION_MINOR
    openupdater::API_VERSION_PATCH
    openupdater::API_VERSION

Version 2.0 is a major API release. Existing 1.x entry points remain available, but new integrations should use the manager APIs for multi-component updates.

## Discovery

The high-level discovery entry point is:

    openupdater::Updater::discover_updates(
        const openupdater::UpdateDiscoveryRequest&)

Discovery checks multiple components without downloading or installing packages. AvailableUpdate contains the component, current/available versions, compatible asset, platform, architecture, SHA-256 digest when available, optional detached signature URL, and download URL.

## Selection

Selection remains separate from discovery:

    openupdater::UpdateSelection selection;
    selection.select("Core");
    selection.select("GUI");

Only selected components are direct roots of the update plan. Dependencies can add additional update components automatically.

## Component manifest

Manifest now supports optional component sections while preserving the original three-key format.

Example:

    application=DemoApp
    version=2.0.0
    package=DemoApp-2.0.0.zip

    [component:Runtime]
    version=2.1.0
    dependencies=

    [component:Core]
    version=2.0.0
    dependencies=Runtime>=2.0.0

    [component:GUI]
    version=2.0.0
    dependencies=Core>=2.0.0,Runtime>=2.0.0

A component dependency has the form component>=minimum-version. Component metadata describes the target package version and its required minimum dependency versions.

## Update planning

The v2.0 planning API is:

    openupdater::UpdateManagerRequest request{
        discovered_updates,
        selection,
        manifest.components,
        installed_components,
        "./updates"
    };

    const auto plan =
        openupdater::Updater::build_plan(request);

UpdateManagerRequest contains:

- updates — discovered update candidates;
- selection — explicitly selected components;
- metadata — target component versions and dependency requirements;
- installed — currently installed component versions;
- destination — installation directory;
- options — verification, backup, rollback and HTTP-header policy.

build_plan() performs preflight validation without downloading or installing anything.

The planner:

1. validates component and version metadata;
2. rejects duplicate components;
3. rejects missing metadata;
4. detects dependency cycles;
5. checks minimum dependency versions;
6. automatically includes available updates required by selected components;
7. produces a topological installation order with dependencies before dependents;
8. rejects ambiguous installation targets.

If a required dependency is already installed at or above its minimum version, its update is not required. If no suitable installed or available version exists, ErrorCode::DependencyFailed is thrown.

## Atomic transaction

Execute a validated plan with:

    const auto report =
        openupdater::Updater::apply_plan(request);

apply_plan() has two phases.

### Phase 1 — complete preflight

Every package is downloaded into .openupdater/staging.

For every package, OpenUpdater performs:

1. download;
2. SHA-256 verification when enabled and a digest is available;
3. Ed25519 signature verification when enabled.

No installation or backup occurs during this phase. If any package fails, all staged files are removed and the destination is left untouched.

### Phase 2 — activation

Packages are installed in dependency order. Each replacement is staged and backed up using the existing BackupManager.

If activation of a later component fails, previously activated components are rolled back in reverse order. A component that had no pre-existing file is removed during transaction rollback.

Transactional execution requires:

- UpdateOptions::backup_existing == true;
- UpdateOptions::automatic_rollback == true.

This requirement prevents the manager from silently creating a partially updated, non-rollbackable state.

UpdateTransactionReport contains the per-component UpdateReport values and the final installation order.

## Package signatures

OpenUpdater 2.0 retains the v1.4 detached Ed25519 mechanism.

A GitHub release can publish:

    Core-windows-x64.zip
    Core-windows-x64.zip.sig

The signature is verified over the raw 32-byte SHA-256 digest of the package. The trusted public key is supplied by the application and is never downloaded from GitHub.

Enable verification with:

    openupdater::UpdateOptions options;
    options.verify_signature = true;
    options.trusted_public_key = "<64 hexadecimal characters>";

## Error model

UpdateError::code() can report:

- InvalidArgument
- InvalidVersion
- UpdateCheckFailed
- DownloadFailed
- VerificationFailed
- InstallationFailed
- RollbackFailed
- DependencyFailed
- TransactionFailed

DependencyFailed means that dependency metadata is invalid, missing, cyclic, or unsatisfied. TransactionFailed means a transaction-level operation failed without a more specific verification or rollback error.

## Legacy APIs

Existing lower-level APIs remain available:

- Version
- Manifest and load_manifest
- Downloader
- GitHubReleasesProvider
- BackupManager
- Updater::check
- Updater::install
- Updater::update_from_github
- Updater::update
- Updater::update_selected

For new multi-component workflows, use:

    discover_updates()
        -> UpdateSelection
        -> build_plan()
        -> apply_plan()
