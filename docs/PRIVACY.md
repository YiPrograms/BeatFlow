# Privacy

BeatNext has no accounts, Google sign-in, telemetry, analytics, advertising, crash upload, cookies, or project-operated backend.

For Up Next, the headset sends anonymous search and radio requests directly to YouTube Music and map metadata/download requests directly to BeatSaver. These services receive normal network information such as the headset's IP address and request headers. YouTube Music's InnerTube interface is unofficial and can change without notice.

BeatNext stores only local settings, bounded anonymous response caches, and temporary staged downloads under:

```text
/sdcard/ModData/com.beatgames.beatsaber/Mods/BeatNext/
├── settings.json
├── cache/music/
├── cache/maps/
└── staging/
```

Downloaded maps are published to SongCore's custom-song directory. Removing BeatNext's mod-data directory clears its settings, caches, and staging files but does not delete installed custom songs.
