# Release process

Publishing is a deliberate maintainer action. A successful build or merge does not publish BeatFlow.

1. Complete the Quest 3 checklist in [Testing](TESTING.md) and attach its record to the release issue.
2. Confirm README screenshots and compatibility claims match the tested build.
3. Update `CHANGELOG.md`, remove the unreleased label for the version, and keep `qpm.json`, CMake,
   `mod.template.json`, and the tag version aligned.
4. Configure the repository's `BEATFLOW_OAUTH_CLIENT_ID` and `BEATFLOW_OAUTH_CLIENT_SECRET` secrets,
   then run formatting, portable tests, the pinned Quest build, and packaging from a clean checkout.
5. Verify `dist/SHA256SUMS`, inspect `mod.json`, install the exact QMOD from `dist/`, and repeat a short
   connect/browse/download/play/finish/next smoke test.
6. Create a signed tag and run the manual release workflow for that tag.
7. Upload the QMOD, checksums, detached debug symbols, compatibility metadata, release notes, and real
   screenshots. Keep symbols separate from the QMOD.
8. Publish only after another maintainer verifies the checksum and package contents.

If device authorization, token refresh, liked-playlist access, anonymous radio, SongDetails lookup, or
BeatSaver download/opening fails, stop the release and document the precise blocker. Do not replace
the headset implementation with a hosted backend or present fixture UI as live integration.
