# Contributing to BeatFlow

Thank you for improving BeatFlow. Small, focused pull requests are easiest to review. Open an issue
before a large UX or architecture change so maintainers and contributors can agree on the behavior.

## Before opening a pull request

1. Build and test the portable target with the commands in [Development](docs/DEVELOPMENT.md).
2. Run `scripts/check-format.sh`.
3. Add a focused fixture or lifecycle test when behavior changes. Sanitize provider fixtures and remove
   account IDs, tokens, cookies, client credentials, and private playlist data.
4. Build the pinned Quest target when changing `src/quest`, assets, dependencies, or packaging.
5. Describe the user-visible result, validation performed, and any headset testing in the PR template.

## Project conventions

- Types use `PascalCase`; functions and variables use `camelCase`; constants use `kPascalCase`.
- Each class owns one clear responsibility. Add an interface only at an external boundary.
- Portable code cannot include Unity, IL2CPP, BSML, or Quest headers.
- Pass dependencies through constructors. Do not introduce global service lookup.
- Background work returns plain values. Only the Quest layer schedules UI callbacks.
- Every asynchronous UI request needs cancellation and a generation check before publishing results.
- Return `Outcome<T>` with an actionable `ServiceError`; do not use exceptions for routine failures.
- Comments explain a constraint or non-obvious decision. They do not restate the code.
- Never log or commit credentials, OAuth tokens, authorization codes, cookies, or raw personal feeds.

The repository uses the [Contributor Covenant](CODE_OF_CONDUCT.md). By participating, you agree to
follow it.
