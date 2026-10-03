# Development

BeatFlow uses C++20, CMake, Ninja, QPM, and Android NDK `27.3.13750724`. The Quest dependency versions
in `qpm.json` are pinned from the BetterSongSearchQuest release compatible with Beat Saber
`1.40.8_7379`.

## Portable setup

Install CMake 3.21 or newer, Ninja, a C++20 compiler, Git, Python 3, and clang-format. The first CMake
configure may fetch nlohmann/json if it is not installed system-wide.

```sh
cmake --preset portable-debug
cmake --build --preset portable-debug
ctest --preset portable-debug
scripts/check-format.sh
```

Portable code belongs under `include/beatflow/core`, `src/core`, `include/beatflow/services`, or
`src/services`. It must continue to compile without QPM and Quest headers.

## Quest setup

Install QPM for Quest mod development, then restore this repository's lock-resolved dependencies and
the pinned NDK:

```sh
qpm restore
qpm ndk resolve -d
export BEATFLOW_OAUTH_CLIENT_ID='your TVs and Limited Input devices client ID'
export BEATFLOW_OAUTH_CLIENT_SECRET='your client secret'
cmake --preset quest-release
cmake --build --preset quest-release
```

Create the OAuth client in Google Cloud using the **TVs and Limited Input devices** application type,
following [Google's device authorization guide](https://developers.google.com/identity/protocols/oauth2/limited-input-device).
The values are compiled into the mod so installed users can start the device flow directly from
BeatFlow. Keep them in local environment variables or repository secrets; never commit them or print
them in build logs. A build without these values still supports anonymous Up Next, but its personalized
sign-in action reports that the release client is missing. The release workflow refuses to package a
tag unless both secrets are configured.

QPM generates `qpm_defines.cmake`, `extern.cmake`, `qpm.shared.json`, `extern/`, and `shared/`. These
local resolution outputs are ignored by Git. `ndkpath.txt` can point CMake at the downloaded NDK.

The resulting library is `build/quest/libBeatFlow.so`. It targets Android API 24 and `arm64-v8a`.

## Package a QMOD

After a RelWithDebInfo Quest build:

```sh
python3 scripts/package.py --build-dir build/quest
sha256sum --check dist/SHA256SUMS
unzip -p dist/BeatFlow-0.1.0.qmod mod.json
```

Packaging leaves the build output untouched. It emits a stripped QMOD, detached debug symbols, and
checksums under `dist/`. Keep debug symbols with the release artifacts so native crash addresses can be
symbolicated.

## Naming and ownership

Use `PascalCase` for types, `camelCase` for functions and variables, and `kPascalCase` for constants.
Prefer automatic storage and `std::unique_ptr`; use `std::shared_ptr` only for state that must survive an
asynchronous handoff. `CompositionRoot` owns production services. Unity components are owned by Unity
and represented with `UnityW` or the established BSML helper types.

One class should answer one question. For example, archive policy belongs in `ZipArchiveValidator`,
installation state belongs in `QuestSongLibrary`, and recommendation identity belongs in `Matcher`.

## Errors, logging, and secrets

Expected failures return `Outcome<T>`. Choose the narrowest `ErrorCode`, write a player-facing message,
and mark retryable only when repeating the same action can reasonably work. Catch exceptions at parser,
filesystem, or worker boundaries and translate them there.

Log lifecycle milestones and sanitized error categories. Never log request authorization headers,
device codes, OAuth bodies, credentials, tokens, cookies, raw personalized responses, or cache bodies.
Fixtures must be synthetic or sanitized.

## Dependencies

Add an interface before adding an SDK directly to the portable core. Pin Quest packages in `qpm.json`,
run `qpm restore`, verify the arm64 link, update `mod.template.json` for every runtime dependency, and
record license information in `THIRD_PARTY_NOTICES.md`.
