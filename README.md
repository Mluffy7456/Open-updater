# OpenUpdater

Open-source, cross-platform application updater written in modern C++20.

## Status

v0.2.0 — HTTP/HTTPS Downloader

Features:
- semantic numeric version comparison;
- update manifest parsing;
- update availability checks;
- HTTP/HTTPS file downloading;
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

The check command returns exit code 10 when an update is available and 0 when the current version is up to date.

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
