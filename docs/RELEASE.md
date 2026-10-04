# Release process

Publishing is a deliberate maintainer action. A successful build or merge does not publish BeatNext.

1. Complete the Quest 3 checklist in [Testing](TESTING.md) and attach its record to the release issue.
2. Confirm README screenshots and compatibility claims match the tested build.
3. Update `CHANGELOG.md`, remove the unreleased label, and align the CMake, QPM, manifest, compatibility, and tag versions.
4. Run formatting, portable tests, the pinned Quest build, and packaging from a clean checkout.
5. Verify `dist/SHA256SUMS`, inspect `mod.json`, install the exact QMOD from `dist/`, and repeat a pause/results/download/select smoke test.
6. Create a signed tag and run the manual release workflow for that tag.
7. Upload the QMOD, checksums, detached debug symbols, compatibility metadata, release notes, and real screenshots. Keep symbols separate from the QMOD.
8. Publish only after another maintainer verifies the checksum and package contents.

If anonymous radio, SongDetails lookup, BeatSaver download, SongCore refresh, or verified Solo selection fails, stop the release and document the blocker. Do not replace the headset implementation with a hosted backend or present fixture UI as live integration.
