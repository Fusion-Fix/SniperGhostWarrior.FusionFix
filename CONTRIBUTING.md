# Contributing

## Development flow

1. Open an issue (bug/feature/performance) before major work.
2. Work in a topic branch.
3. Keep changes scoped and include reproduction steps where possible.

## Reverse-engineering notes

Do not leave findings only in chat logs.

- Put durable findings in `docs/research/`.
- Use one file per topic (pattern scans, class layouts, offsets, call flows).
- Include game version/build and evidence (addresses, signatures, traces, screenshots).
- Update notes when assumptions change.

Suggested filename format:

`docs/research/YYYY-MM-DD-<topic>.md`

## Code review checklist

- Works on target architecture (x86/x64).
- No pointer truncation (`DWORD` casts for addresses are not allowed on x64).
- Premake and CI stay in sync with selected settings.
- New dependencies are declared as submodules and wired in premake.
