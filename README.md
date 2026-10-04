# BeatNext

BeatNext turns the song you are playing into a queue of Beat Saber maps. Pause a solo song or finish it successfully, then browse up to 20 related tracks in a large panel to the right of Beat Saber's own UI. Select a track to see its full YouTube title, artists, mapper, rating, artwork, and available Standard difficulties. BeatNext downloads the map when needed and opens Solo with that map selected.

**Developer preview:** BeatNext currently targets Beat Saber `1.40.8_7379` on Meta Quest. It uses YouTube Music's public InnerTube search and radio operations anonymously. There is no account connection, Google sign-in, backend, or telemetry.

## What playing with BeatNext feels like

1. Start any solo built-in or custom song with usable title and artist metadata.
2. Open Pause to browse Up Next while the run is still active, or finish the song to see the same panel beside the score screen.
3. Select any result in the scrollable list to hear its preview. BeatNext preserves the complete Unicode title and your scroll position while showing the matching BeatSaver map in the details column.
4. Choose **Download**, then **Play** when the map is ready. Already downloaded maps show **Play** immediately.
5. From Pause, confirm before leaving the current run. From Results, BeatNext closes the score screen normally.
6. BeatNext refreshes SongCore, opens Solo, and selects the requested map so you can choose a difficulty and press Play.

The normal Pause and Results controls remain usable. Up Next never appears in multiplayer, after a failed run, or when its screen is disabled in settings.
While recommendations are being prepared, the panel shows the matched YouTube Music track and live matching progress. Use the arrow beside that track to open it in YouTube Music.

## Compatibility

| Headset | Status |
|---|---|
| Meta Quest 3 | Primary target; developer validation in progress |
| Meta Quest 3S | Designed to work; unverified |
| Meta Quest 2 | Designed to work; unverified |
| Meta Quest Pro | Designed to work; unverified |
| Meta Quest 1 | Unsupported |

| Component | Version |
|---|---|
| Beat Saber | `1.40.8_7379` |
| Mod loader | Scotland2 |
| Android NDK | `27.3.13750724` |

## Installation

Install the release `.qmod` with a Quest mod manager. The package declares compatible versions of beatsaber-hook, BeatSaverPlusPlus, BSML, Custom Types, Paper, SongDetails, SongCore, and WebUtils; the manager should install missing dependencies.

1. Confirm Beat Saber is exactly `1.40.8_7379` and already modded with Scotland2.
2. Download `BeatNext-<version>.qmod` from [GitHub Releases](https://github.com/YiPrograms/BeatNext/releases).
3. Install the QMOD and its declared dependencies.
4. Start Beat Saber. **Mods → BeatNext** should open the two-option settings page.

## Settings

BeatNext has exactly two settings, both enabled by default:

- **Show Up Next on score screen**
- **Show Up Next on pause screen**

Settings are stored under `/sdcard/ModData/com.beatgames.beatsaber/Mods/BeatNext/settings.json`.

## Troubleshooting

**The panel says the song cannot be identified.** The level must provide a useful title and artist. BeatNext searches YouTube Music using both and uses the best provider result as the radio seed when a strict identity match is unavailable.

**No confident BeatSaver matches were found.** YouTube radio can return tracks that have no compatible Standard BeatSaver map. BeatNext rejects unrelated recordings, automaps, unsupported characteristics, and maps with unavailable requirements.

**A download failed.** Check the headset network connection and try again. BeatNext downloads into staging, validates the ZIP and `Info.dat`, and only publishes a complete map.

**The downloaded map did not open.** Wait for SongCore to finish refreshing. If Solo cannot initialize or verify the selected hash, BeatNext reports an error without blocking Beat Saber navigation.

**The service is offline.** Cached search, radio, and BeatSaver responses remain usable and are labeled cached/offline.

## Privacy and removal

BeatNext has no telemetry, account system, cookies, or project-operated server. Requests go directly from the headset to YouTube Music and BeatSaver. See [Privacy](docs/PRIVACY.md).

Uninstall the QMOD to remove the mod. Delete `/sdcard/ModData/com.beatgames.beatsaber/Mods/BeatNext/` to remove settings, caches, and incomplete staging data. Downloaded custom songs are managed separately by SongCore.

## Contributing and development

Start with [CONTRIBUTING.md](CONTRIBUTING.md). Architecture, build instructions, testing, release procedure, security reporting, dependency notices, and changes live in:

- [Architecture](docs/ARCHITECTURE.md)
- [Development](docs/DEVELOPMENT.md)
- [Testing](docs/TESTING.md)
- [Release process](docs/RELEASE.md)
- [Security](SECURITY.md)
- [Third-party notices](THIRD_PARTY_NOTICES.md)
- [Changelog](CHANGELOG.md)

BeatNext is available under the [MIT License](LICENSE).
