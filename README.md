# BeatFlow

BeatFlow turns the song you just played into the next Beat Saber map. It resolves the current song
through YouTube Music's anonymous InnerTube interface, follows its radio queue, finds confident
BeatSaver matches, and shows the choices on the solo pause and results screens.

**A Google account is not required for Up Next.** Connecting YouTube Music is optional and adds the
personalized **For You** feed using your Home recommendations, listening history, likes, and library
signals.

> [!WARNING]
> BeatFlow 0.1 is a developer preview for Beat Saber `1.40.8_7379`. The code, portable tests, Quest
> build, and QMOD packaging are available; the complete on-device loop still needs recorded Quest 3
> validation before this preview should be described as release-ready.

## What playing with BeatFlow feels like

1. Start any solo song. BeatFlow resolves its title, artist, and duration in the background.
2. Pause to glance at the prefetched Up Next list without leaving the level.
3. Finish the song normally. The results buttons remain usable immediately.
4. Choose one of up to three **Up Next** maps, or open **See more**.
5. BeatFlow downloads and validates the map when needed, refreshes SongCore, and opens Beat Saber's
   normal song detail screen. You choose the difficulty and press Play.
6. Finish the next song and continue the chain.

From the Mods menu, **BeatFlow** opens **For You**. Connect YouTube Music there to browse personalized
recommendations. Results appear progressively and show artwork, song, artist, mapper, rating,
difficulties, and install status. Difficulty and notes-per-second filters start open: all difficulties,
any NPS, Standard characteristic.

Real headset screenshots will be added after the Quest 3 validation pass. The project does not use
mock screenshots to imply that hardware validation has happened.

## Compatibility

| Component | Status |
| --- | --- |
| Beat Saber `1.40.8_7379` | Build target; pinned |
| Mod loader | Scotland2 |
| Meta Quest 3 | Primary validation target; end-to-end run pending |
| Meta Quest 2 | Designed to be compatible; unverified |
| Meta Quest 3S | Designed to be compatible; unverified |
| Meta Quest Pro | Designed to be compatible; unverified |
| Meta Quest 1 | Deferred |
| PCVR | Out of scope for 0.1 |

Compatibility follows the game version and mod dependency set more than the headset model. BeatFlow
uses Android APIs available on the supported Quest generation, but each model stays marked
**unverified** until the same install-to-Up-Next loop is tested on that hardware.

## Install

This preview expects an already modded Quest installation of Beat Saber `1.40.8_7379` using
Scotland2.

1. Download `BeatFlow-0.1.0.qmod` and `SHA256SUMS` from the matching release or build them from source.
2. Verify the checksum:

   ```sh
   sha256sum --check SHA256SUMS --ignore-missing
   ```

3. Install the QMOD with your Quest mod manager. Its manifest declares the required versions of
   beatsaber-hook, BeatSaverPlusPlus, BSML, custom-types, Paper, SongCore, and WebUtils.
4. Start Beat Saber and confirm that **BeatFlow** appears in the Mods menu.

Do not install this build on another Beat Saber version. Native Quest mods are version-specific.

## Optional: connect YouTube Music

Up Next uses anonymous InnerTube `search` and `next` requests. Follow this section only if you want the
personalized For You feed.

1. Open **Mods → BeatFlow → Sign in with Google**.
2. BeatFlow opens Google's device sign-in page in the Quest browser and shows the authorization code
   in Beat Saber.
3. Enter the displayed code and approve access. Return to Beat Saber; BeatFlow continues polling and
   finishes automatically.

The **Browser** button reopens the same Google page while its code is active. BeatFlow validates an
authenticated Home request and a radio request before reporting the connection as ready. You do not
need to create or copy an OAuth JSON file when using an official BeatFlow build.

Google requires device apps to use a **TVs and Limited Input devices** OAuth client, as described in
its [limited-input device authorization guide](https://developers.google.com/identity/protocols/oauth2/limited-input-device).
Release maintainers inject that client at build time; its metadata is part of the installed mod, while
each player's access and refresh tokens are encrypted separately with Android Keystore. Self-build
instructions are in [Development](docs/DEVELOPMENT.md).

## Use BeatFlow

### Up Next

Up Next works without an account for built-in and custom solo levels that expose enough song metadata.
BeatFlow prefetches metadata after the level starts and does no synchronous network or disk work on the
gameplay thread. The pause panel is read-only, uses text instead of decoding artwork during play, and
never starts a map installation. The results shelf appears only after a successful finish; it does not
appear after a fail, quit, or multiplayer game.

Selecting a result downloads it if needed. Archives are checked for unsafe paths, bounded size, and
Beat Saber metadata, then extracted into staging and moved into the SongCore folder as one final step.
If resolution fails, the results panel explains why; **See more** still opens BeatFlow, where For You is
available after connecting an account.

The BeatFlow page has independent **Show Up Next after song** and **Show Up Next on pause** options.
Both are enabled by default and saved locally. Turning off the pause option takes effect for the next
level; the results option takes effect on the next successful finish.

### For You and filters

Open **Mods → BeatFlow** and connect YouTube Music. Select **Refresh** to reload personalized Home
shelves. BeatFlow expands only a bounded number of album and playlist shelves, skips non-song cards,
and shows maps as matching completes.

- **Difficulty** cycles through All, Hard, Expert, and Expert+.
- **NPS** cycles through Any, up to 4, 4–6, and 6+ notes per second.
- The preview supports Standard maps and rejects maps with unavailable requirements.
- **Cancel** stops the active page request. Closing the page also discards late results.

When a cached response is used during an outage, the page says **Cached/offline**. Filters that remove
every map produce a separate explanation from a recommendation feed with no confident matches.

## Troubleshooting, privacy, and removal

**BeatFlow says that its Google OAuth client is missing.** Anonymous Up Next still works. An official
release with this message was packaged incorrectly; report its version and checksum. Self-builders
must provide the build environment variables documented in [Development](docs/DEVELOPMENT.md).

**The code expired or authorization was denied.** Select Connect to start a new device flow. BeatFlow
honors Google's polling interval, expiration, and slow-down responses.

**A song has no Up Next shelf.** BeatFlow needs a usable title and artist and a confident YouTube and
BeatSaver identity match. It deliberately rejects a popular but unrelated remix, cover, live version,
or shortened recording.

**A download failed.** Retry from the card. Interrupted work stays in staging and cannot publish a
partial custom song. Maps with unsupported requirements are excluded before download.

**Disconnect** removes encrypted player tokens and personalized recommendation caches.
**Clear local data** also clears anonymous recommendation and BeatSaver caches plus download staging.
Downloaded custom songs remain in SongCore's custom-level folder.

BeatFlow has no telemetry. Requests go directly from the headset to Google/YouTube and BeatSaver.
Ordinary caches are versioned and bounded; secrets are stored separately with Android Keystore. See
[Privacy](docs/PRIVACY.md) for the data inventory and unofficial-interface caveat.

To uninstall, remove BeatFlow through the same mod manager used to install it. Remove
`/sdcard/ModData/com.beatgames.beatsaber/Mods/BeatFlow` as well if you want to delete all local BeatFlow
account data and caches.

## Contributing and development

The portable core is ordinary C++20 and can be developed without Beat Saber or a headset. Start with
[Contributing](CONTRIBUTING.md), then read [Development](docs/DEVELOPMENT.md),
[Architecture](docs/ARCHITECTURE.md), and [Testing](docs/TESTING.md). Release maintainers should also
follow [Release process](docs/RELEASE.md).

BetterSongSearchQuest's `1.40.8` release provided the reference dependency set. BeatFlow uses
ytmusicapi's public documentation as a behavioral reference for the small InnerTube surface it needs;
it does not bundle either project. Full attribution is in [Third-party notices](THIRD_PARTY_NOTICES.md).

Changes are recorded in the [changelog](CHANGELOG.md). Security reports follow
[SECURITY.md](SECURITY.md). BeatFlow is available under the [MIT License](LICENSE).
