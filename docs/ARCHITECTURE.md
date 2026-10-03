# Architecture

BeatFlow has three layers and one composition point. Dependencies point inward, and the portable core
has no knowledge of Beat Saber, Unity, IL2CPP, Android, or BSML.

```text
Quest hooks and BSML
        │
        ▼
CompositionRoot ── service adapters ── YouTube / Google / BeatSaver / storage
        │
        ▼
portable recommendation core
```

## Portable core

`include/beatflow/core` and `src/core` contain plain value types and deterministic policy:

- `TextNormalizer` preserves Unicode and meaningful recording markers while removing presentation
  noise.
- `Matcher` proves song identity before applying map quality and difficulty suitability.
- `RecommendationEngine` orchestrates Home, search, radio, map lookup, filtering, deduplication, and
  session exclusions.
- `CancellationSource` and `CancellationToken` carry cooperative cancellation without a platform type.
- `Outcome<T>` and `ServiceError` make user-actionable failure categories explicit.

Scoring constants live with the matcher instead of being spread across adapters. A candidate must pass
the identity threshold before popularity, votes, or curation can improve its final rank. Remix, cover,
live, instrumental, acoustic, nightcore, sped-up, slowed, and shortened markers participate in identity
so a popular alternate recording cannot outrank the intended song.

## Service adapters

`include/beatflow/services` and `src/services` implement external boundaries:

- `YouTubeMusicProvider` sends anonymous InnerTube `search` and `next` requests. Authenticated `browse`
  is used only for personalized Home shelves.
- `OAuthClient` owns device authorization, expiry, polling, refresh, and disconnect semantics.
- `BeatSaverCatalog` translates map search responses into portable candidates.
- `RetryingHttpClient` applies bounded retries, cancellation, and `Retry-After` delays.
- `AtomicJsonCache` provides bounded, atomic files and rebuilds corrupt entries at the adapter boundary.
- `ZipArchiveValidator` inspects central-directory paths and size bounds before extraction.
- `WorkerQueue` bounds concurrency and pending work.

The adapters depend on `HttpClient`, `CredentialStore`, and `CacheStore` interfaces. Tests replace those
boundaries with in-memory fakes; production binds them to Quest implementations.

## Quest integration and composition

`CompositionRoot` is the single production composition point. It owns the HTTP stack, caches,
credentials, providers, recommendation engine, SongCore adapter, worker queue, cancellation sources,
and per-session played hashes. Other Quest classes request operations from this root; they do not find
services through a general registry.

`LevelLifecycle` hooks solo level startup, pause presentation, and successful results activation.
Startup submits anonymous metadata prefetch. The pause hook adds a read-only, text-only panel and the
results hook adds an interactive `RecommendedNextPanel`; neither replaces Beat Saber's controls. The
menu flow uses `ForYouViewController` for personalized and expanded results.

`QuestSongLibrary` validates a downloaded ZIP, extracts into a unique staging directory, checks the
resulting tree and `Info.dat`, and renames the completed directory into SongCore's custom-level folder.
It then asks SongCore to refresh and hands the hash to the normal level-selection UI.

`QuestCredentialStore` receives the app's Google limited-input-device OAuth client metadata from the
generated build configuration. It keeps a one-time encrypted file import as a compatibility path for
older self-builds. `AndroidKeystore` creates a non-exportable AES key for player OAuth tokens. Ordinary
configuration and caches never contain those tokens.

## Threading and lifecycle

Network, parsing, matching, cache access, downloads, and SongCore refresh run on a bounded two-thread
queue. Quest worker threads attach to IL2CPP for the duration of a task. BSML's main-thread scheduler is
the only path for UI mutations.

Interactive and gameplay-prefetch operations have separate cancellation sources and monotonically
increasing generations. Starting another request, closing the view, or starting another level cancels
the old source. Every asynchronous UI publication checks both source identity and generation, which
prevents a late response from a previous scene from replacing current state.

Gameplay prefetch stops at metadata and matching. Artwork decode, archive installation, SongCore
refresh, and song selection happen only in menu or results scenes.

## Caching and failure behavior

Anonymous music, personalized music, and BeatSaver data use separate bounded cache directories.
Personalized entries include an opaque namespace derived from the account's refresh token, preventing
one connected account from reading another account's cached feed. Provider entries wrap the raw
response with a schema version and timestamp. Network and refresh errors may fall back to a valid entry;
resulting tracks and maps carry a stale bit that reaches the UI as `Cached/offline`. Invalid envelopes
are removed and rebuilt.

Errors include a category, message, retryability, and optional retry delay. UI text tells the player
whether to connect, retry, change filters, or continue without Up Next. Beat Saber navigation never
depends on a recommendation request succeeding.

## Extending the design

A new music source implements `MusicProvider`; a new map source implements `MapCatalog`. Keep provider
response shapes and authentication rules in the adapter. Add portable behavior only when it represents
source-independent recommendation policy. Avoid adding a plugin framework or another composition root.
