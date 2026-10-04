# Development

BeatNext uses C++20, CMake, Ninja, QPM, and Android NDK `27.3.13750724`. Quest dependency versions are pinned in `qpm.json` for Beat Saber `1.40.8_7379`.

## Portable build

```sh
cmake --preset portable-debug
cmake --build --preset portable-debug
ctest --preset portable-debug
scripts/check-format.sh
```

## Quest build

```sh
qpm restore
qpm ndk resolve -d
cmake --preset quest-release
cmake --build --preset quest-release
python3 scripts/package.py --build-dir build/quest
```

The library is `build/quest/libBeatNext.so`. Packaging produces the QMOD, detached debug symbols, compatibility metadata, and SHA-256 checksums under `dist/`.

## Code conventions

- Namespaces and targets use `beatnext`; product-facing text uses `BeatNext`.
- Keep Unity and IL2CPP types in `src/quest` and plain data in the portable core.
- Prefer explicit ownership and constructor injection. Do not introduce global service locators.
- Do not perform network or disk work on the gameplay thread.
- Every asynchronous result must be cancellable and associated with a level generation.
- Return actionable `ServiceError` values at external boundaries.
- Add fixture tests for provider wire formats and focused tests for matching or session transitions.
- Never log response bodies, cache contents, or local paths containing user data.
