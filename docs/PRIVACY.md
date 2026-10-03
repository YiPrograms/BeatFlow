# Privacy

BeatFlow has no telemetry, analytics, advertising, crash upload, or project-operated backend.

## Without an account

Up Next sends song title and artist search terms to YouTube Music's anonymous InnerTube `search`
endpoint, then sends the resolved video identifier to its `next` endpoint. Candidate track metadata is
sent to BeatSaver search. These services receive normal network information such as the headset's IP
address. BeatFlow stores bounded response caches locally so useful results can survive an outage.

## With optional YouTube Music connection

The preview imports the player's personal OAuth client ID and secret from `oauth_client.json`. It uses
Google's device authorization flow and the YouTube scope to obtain access and refresh tokens. The
authenticated token is sent only for personalized Home `browse` requests. Search and song radio remain
anonymous and do not depend on the account.

Client credentials and OAuth tokens are separate AES-GCM encrypted files. The encryption key is
generated and retained by Android Keystore. The plaintext import file is deleted after successful
encryption. Personalized response caches are stored separately from anonymous and BeatSaver caches.

## Local files

BeatFlow uses this directory:

```text
/sdcard/ModData/com.beatgames.beatsaber/Mods/BeatFlow/
├── oauth_client.enc       encrypted imported client credentials
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

**Disconnect** removes imported client credentials, tokens, and personalized caches. **Clear local
data** also removes anonymous music and BeatSaver caches and staging files. Removing the entire BeatFlow
mod-data directory after uninstall deletes the same local data.

## Unofficial interface

YouTube Music's InnerTube interface is unofficial and can change without notice. BeatFlow sends only
the operations needed for anonymous search/radio and optional Home personalization. It does not scrape
cookies or ask the player to export browser headers. Review Google's and BeatSaver's privacy terms to
understand their handling of direct requests.
