# Changelog

All notable changes to BeatFlow are documented here. The project follows
[Semantic Versioning](https://semver.org/spec/v2.0.0.html) while its public interfaces stabilize.

## [Unreleased]

### Added

- Anonymous current-song resolution and InnerTube radio recommendations.
- Optional Google device authorization for personalized YouTube Music Home recommendations.
- Quest-browser sign-in with release-provided OAuth client metadata; no user-side JSON copy is needed.
- Identity-first BeatSaver matching with difficulty and NPS filters.
- Progressive For You browser plus configurable pause and post-results Up Next panels.
- Normal-size text in the compact pause and results Up Next panels.
- Direct, cancellable libcurl transport that avoids WebUtils 0.6.9's invalid HTTP status storage.
- Validated staged map installation and normal Beat Saber song-detail handoff.
- Encrypted credential and token storage through Android Keystore.
- Bounded workers, cancellation generations, retry/backoff, and versioned offline caches.
- Portable fixture tests, pinned Quest build, deterministic QMOD packaging, and open-source docs.

### Fixed

- Load Quest's system certificate store for verified HTTPS requests from the bundled libcurl client.
- Keep the Browser action safe when device authorization has not produced a URL.
- Keep the anonymous WEB_REMIX API key off OAuth-authenticated InnerTube requests.
- Resolve shortened and TV-size maps through the matching original recording when an exact edit is absent.

### Changed

- Display the Google device pairing code as a large, dedicated line for easier reading in-headset.

## [0.1.0] - Unreleased developer preview

Quest 3 hardware acceptance remains pending.
