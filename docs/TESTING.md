# Testing

## Automated

Portable tests cover Unicode normalization, aliases, remixes, short versions, missing durations, Standard-map compatibility, requirements, ranking, deduplication, exclusions, anonymous search/radio parsing, malformed responses, stale caches, archive safety, retry behavior, and recommendation-session generations and item states.

CI runs formatting, portable tests, the pinned Quest build, package inspection, and checksum validation.

## Quest 3 acceptance

1. Install BeatNext and dependencies on Beat Saber `1.40.8_7379`.
2. Open **Mods → BeatNext** and toggle each screen independently; restart to verify persistence.
3. Play built-in and custom solo songs. Pause each and confirm the large right-side panel appears without obscuring Pause controls.
4. Scroll a long recommendation list, select several rows, and verify the list keeps its position while full Unicode titles, artists, artwork, mapper, rating, and Standard difficulties update. Verify each selection starts the correct preview, including rapid selection changes.
5. Download from Pause. Cancel the exit confirmation and verify the run remains paused; repeat and confirm exit.
6. Verify Solo opens with the downloaded map selected on the first attempt.
7. Finish a song successfully and repeat installed and uninstalled flows from Results. Verify native result controls remain usable.
8. Fail, quit, and enter multiplayer; verify the Results panel does not appear.
9. During a fresh search, verify the spinner advances through identification, radio loading, and checked-song counts. Verify the matched-track arrow opens YouTube Music.
10. Test network loss, empty radio results, failed archives, and SongCore refresh errors. Beat Saber navigation must remain usable.
11. Repeat several recommendation chains while observing frame time and memory. There must be no synchronous gameplay work, sustained regression, stale panel, or growing memory use.

Quest 2, Quest 3S, and Quest Pro remain unverified until this sequence is recorded on each device.
