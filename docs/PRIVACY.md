# Privacy

BeatFlow has no telemetry, analytics, advertising, crash upload, or project-operated backend.

## Without an account

Up Next sends song title and artist search terms to YouTube Music's anonymous InnerTube `search`
endpoint, then sends the resolved video identifier to its `next` endpoint. Candidate track metadata is
sent to BeatSaver search. These services receive normal network information such as the headset's IP
address. BeatFlow stores bounded response caches locally so useful results can survive an outage.

## With optional YouTube Music connection

The release build contains the app's Google **TVs and Limited Input devices** OAuth client metadata.
Selecting **Sign in with Google** starts Google's device authorization flow, opens its verification
page in the Quest browser, and uses the YouTube scope to obtain access and refresh tokens. The
authenticated token is sent only for personalized Home `browse` requests. Search and song radio remain
anonymous and do not depend on the account.

Player OAuth tokens are stored in an AES-GCM encrypted file whose key is generated and retained by
Android Keystore. Personalized response caches are stored separately from anonymous and BeatSaver
caches. BeatFlow never receives the player's Google password.

## Local files

BeatFlow uses this directory:

```text
/sdcard/ModData/com.beatgames.beatsaber/Mods/BeatFlow/
├── oauth_tokens.enc       encrypted access and refresh tokens
├── settings.json          Up Next display preferences
├── cache/maps/            bounded BeatSaver response cache
├── cache/music/           bounded anonymous InnerTube response cache
├── cache/accounts/        bounded personalized response cache
└── staging/               incomplete map installations
```

Downloaded maps are installed into SongCore's configured custom-level folder and therefore outlive a
BeatFlow data clear.

## Delete data

**Disconnect** removes player tokens and personalized caches. **Clear local data** also removes
anonymous music and BeatSaver caches and staging files. Removing the entire BeatFlow mod-data directory
after uninstall deletes the same local data.

## Unofficial interface

YouTube Music's InnerTube interface is unofficial and can change without notice. BeatFlow sends only
the operations needed for anonymous search/radio and optional Home personalization. It does not scrape
cookies or ask the player to export browser headers. Review Google's and BeatSaver's privacy terms to
understand their handling of direct requests.
