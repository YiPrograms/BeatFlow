# Testing

## Automated checks

Run the portable suite and formatting check on every change:

```sh
cmake --preset portable-debug
cmake --build --preset portable-debug
ctest --preset portable-debug
scripts/check-format.sh
```

Fixture tests cover mixed Home shelves, radio and search parsing, malformed responses, anonymous auth
headers, device authorization, refresh, offline cache fallback, corrupt cache envelopes, map metadata,
multilingual identity, duration boundaries, remix and cover conflicts, duplicates, difficulty/NPS
boundaries, cancellation, server backoff, atomic storage, bounded workers, and unsafe ZIP paths.

Quest integration changes also require:

```sh
qpm restore
cmake --preset quest-release
cmake --build --preset quest-release
python3 scripts/package.py --build-dir build/quest
```

Inspect the generated QMOD manifest and confirm that the native library is arm64, stripped in the QMOD,
and accompanied by detached symbols.

## Quest 3 acceptance pass

Use a clean Beat Saber `1.40.8_7379` installation and record the game version, headset model, OS build,
QMOD checksum, and dependency versions.

1. Install BeatFlow and its declared dependencies; launch and open its Mods menu entry.
2. Without credentials, play a built-in song and a custom song, finish each, and verify Up Next appears.
3. Select an uninstalled recommendation, verify progress, choose a difficulty on the normal details
   screen, play it, finish, and continue for several rounds.
4. Fail a song, quit a song, and play multiplayer. Confirm no shelf appears and normal navigation works.
5. Add personal client credentials, connect with the device URL and code, and verify expiry, cancel,
   token refresh, reconnect, Disconnect, and Clear local data.
6. Browse For You, change every difficulty and NPS filter, select installed and uninstalled maps, and
   verify loading, empty, filtered, offline, cached, retry, and malformed-response states.
7. Interrupt a download and restart Beat Saber. Confirm there is no partial song and a retry succeeds.
8. Switch scenes, close the view during requests, replay a level, and chain recommendations. Confirm no
   stale callback changes the current screen.
9. Compare the same map with prefetch enabled and disabled. Capture frame times and memory across
   repeated rounds; require no synchronous gameplay-thread network/disk work, sustained frame-time
   regression, or growing memory.
10. Capture real screenshots only after the corresponding screen and behavior pass.

Quest 2, 3S, and Pro remain unverified until this same loop is recorded on each model.

## Fixture hygiene

Prefer small synthetic responses that preserve the renderer shape under test. Before committing a real
response, replace account names, IDs, playlist IDs, history, likes, library contents, tokens, cookies,
authorization headers, and unique tracking values. A fixture must never require network access.
