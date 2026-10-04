# Architecture

BeatNext has three layers and one composition point. Dependencies point inward, and Unity or IL2CPP types never enter the portable core.

```text
Quest lifecycle and UI → recommendation session → portable recommendation engine
          ↓                         ↓                         ↓
  SongCore/navigation       downloads and caches      YouTube + BeatSaver ports
```

## Portable core

`RecommendationEngine` accepts a plain `CurrentSong`, resolves it through anonymous YouTube Music search, fetches the track radio, and matches each track against BeatSaver. Identity is established from title, artist, recording markers, and duration before map quality affects ranking. The core returns at most 20 unique map hashes and excludes the current and already-played maps.

`RecommendationSession` is the single observable UI state. Each level starts a new generation. Completion, selection, and download updates must carry that generation, so callbacks from an older level are rejected. Subscribers receive immutable snapshots outside the session lock.

## Services

`YouTubeMusicProvider` implements only anonymous InnerTube `search` and `next`. `SongDetailsCatalog` prefers the local SongDetails database and falls back to BeatSaver search. HTTP retries are bounded and honor server backoff. Anonymous response caches are bounded, versioned JSON envelopes written atomically.

`QuestMapInstaller` implements the narrow `MapInstaller` boundary. It stages and validates archives before publishing them to SongCore's preferred custom-song directory. It rejects unsafe paths, links, excessive file counts, and archives without `Info.dat`. `SongSelectionNavigator` separately owns navigation into Solo.

## Quest integration

`CompositionRoot` constructs the provider, catalog, library, engine, worker queue, and session. It contains no browsing mode, account state, filters, or UI-specific recommendation list.

At solo level start, the lifecycle adapter starts one cancellable recommendation generation. Results and Pause create independent floating-screen hosts for the same `UpNextPanelController`; closing either screen destroys its host and subscription. Network and matching run on the bounded worker queue. Unity updates, SongCore UI transitions, and image work run on the main thread.

`SongSelectionNavigator` owns the post-download transition. It waits until SongCore resolves the hash, closes the source screen through Beat Saber's normal controls, configures Solo, waits for active level-selection controllers, explicitly selects the level, and verifies the selected object.

## Conventions

- Types own one responsibility and receive external dependencies through constructors.
- Portable code uses plain value types and `Outcome<T>` errors.
- UI observes session snapshots; it does not start provider requests or mutate caches directly.
- Worker callbacks carry cancellation plus a generation. Stale results are discarded.
- Comments explain lifecycle ordering, provider quirks, and security decisions rather than restating code.
