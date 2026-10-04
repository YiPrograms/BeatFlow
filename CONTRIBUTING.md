# Contributing

Thanks for helping improve BeatNext.

1. Open an issue for user-visible changes or substantial architecture work.
2. Keep pull requests focused and describe the concrete before/after behavior.
3. Run the portable build, tests, and formatting check.
4. For Quest changes, run the pinned Android build and describe device validation performed.

Keep Unity types out of the portable core, preserve cancellation and generation checks, return actionable errors, and comment lifecycle ordering that is not obvious from the code. Never include private device logs or raw cache bodies in issues, tests, or pull requests.
