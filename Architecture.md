# Architecture and Development Guidelines

Rules for everyone working on this repository.

## Project

djw (DJ-Workstation) is a lean, touch-first DJ application in C++23. It is a complete rewrite of the original Win32 DJ-Workstation, developed from 1998 to 2002. No code from the original is reused.

**`FEATURES.md` is the specification.** Read it before starting any task. Do not implement features that are not listed there, and never implement anything from its non-goals list. If a task seems to require something outside the spec, ask first.

## Current milestone

**M1 MVP** (see FEATURES.md, section 8). M0 Skeleton is done. Work milestone by milestone. Do not start features of a later milestone unless explicitly asked.

## Hard rules

### Architecture
- `core/` must never include anything from `player-ui/` or SDL. The core builds and runs headless.
- The frontends in `player-ui/` talk to the core only through `core/api/`: setup (adding decks, starting audio) through the `api::Core` interface, while running only through the command queue and state snapshots. The music library through the `api::Library` interface (`api::openLibrary`), its saved playlists through `api::Playlists` (`Library::playlists()`). No direct calls into engine or library internals.
- Platform-specific code lives in `platform/` or behind an interface, never scattered with `#ifdef` across the core.

### Audio thread
The audio callback and everything it calls must be real-time safe:
- no heap allocation or deallocation (`new`, `delete`, `malloc`, growing `std::vector`, `std::string`, `std::function` with captures)
- no locks (`std::mutex`, condition variables), only atomics and lock-free queues
- no I/O, no logging, no syscalls
- no exceptions
- bounded execution time

Mark audio-thread functions with a comment `// [audio-thread]` so they are easy to audit. When in doubt whether something is RT-safe, it is not.

### Track loading
- A loaded track is decoded completely into memory as interleaved stereo float32 at the engine sample rate, so cue, hot cues, seeking, loops and reverse playback (scratch) work at any position without latency. The waveform overview uses the same samples.
- Decoding runs on the decoder thread in chunks of a few seconds, from the start to the end. Playback can start as soon as the first chunk is ready. A seek into a part that is not decoded yet makes the decoder decode that part first; only then a short gap is possible.
- The audio thread reads the decoded chunks directly. Chunks are published to it lock-free and stay unchanged while the track is loaded; they are freed outside the audio thread after the track is unloaded.
- Memory per track: about 23 MB per minute at 48 kHz (a 5-minute track about 115 MB).
- The size held in memory has a configurable limit, as in VirtualDJ. Tracks above it are streamed from disk through a ring buffer (`decode::StreamingSource`); every seek there (also a jump on the waveform, turning keylock on or off, the scrub loop) has a gap of some 50 ms while the stream refills, and reverse playback is limited to what is buffered. The default limit is 20 minutes of track length (about 460 MB), so DJ tracks including extended versions are held in memory; longer mixes are streamed. If the memory cannot be had the track is streamed as well.
- Implemented in `decode::MemoryTrack` and `decode::StreamingSource`; `decode::makeTrack` chooses between them.

### Dependencies
- Only use libraries listed in FEATURES.md section 6.
- Ask before adding any new dependency.
- Dependencies are declared via CMake `FetchContent` with pinned versions (tag or commit hash) and `FIND_PACKAGE_ARGS`: an installed system or vcpkg package is used if found, otherwise the pinned version is downloaded. Exception: libraries without a CMake package config (miniaudio, SQLite) are always downloaded and built from their sources by our own CMake target; a download by URL is pinned by its SHA-256 hash. Single-header libraries and assets such as the UI font (`third_party/fonts/`) may be vendored in `third_party/` with their license file.
- Every dependency must be compatible with AGPLv3.

### Licensing
- Project license: AGPL-3.0-or-later.
- Every new source file starts with:
  ```cpp
  // SPDX-License-Identifier: AGPL-3.0-or-later
  // Copyright (C) 2026 Niko Sagiadinos
  ```
- Files taken from third parties (e.g. xwax timecoder) keep their original headers and license unchanged.

## Design principles

- Single responsibility: one class has one reason to change. If a description needs "and", split it.
- Dependencies on the outside world (clock, audio device, file system, MIDI, database) are interfaces passed in through the constructor. This keeps modules testable with fakes and replaceable per platform.
- Depend on interfaces, not on concrete implementations, across module boundaries. Inside a module, concrete types are fine.
- Composition over inheritance. Inheritance only for implementing an interface.
- Keep interfaces small. A consumer must not depend on methods it does not use.
- No duplicated logic: extract it when it appears a second time. Code that only looks alike but changes for different reasons stays separate.
- Exception: the audio path. Real-time safety and performance come before abstraction there, so no virtual calls per sample and no indirection inside inner loops.

## Player windows

- A view (`player-ui/sdl/view.hpp`: player, library, crossfader, playlist) lays itself out in the rectangle it is given and never assumes it starts at the window's top left corner. Tests check this for every view (`test_view_host.cpp`).
- A window is a host: `SdlWindow` shows the views of its `ViewHost`, each in its own area, without overlap. Pointer input goes to the widgets of all its views, the wheel and the right mouse button to the view under the pointer, typed text to the view a pointer last went down on.
- Two window modes, chosen in View and taking effect at the next start; the layout file keeps the mode and a layout for each mode:
  - Separate windows: every element has its own window (one view filling its host).
  - One window (MDI): one fullscreen host on the main display. Every element is a part of it in a `PaneFrame` (title bar, close button for players, crossfader and playlist). `arrangeMdi` places the parts automatically and without overlap: players in a row with the crossfader after player 1, the library below, the playlist to its right. The row shrinks as a whole when it does not fit. Closing the host ends the program.
- Still open for the MDI mode: one host per monitor, moving parts with magnetic snapping, a minimum size per part, scrolling when even that does not fit. Window positions are not used for it, since Wayland gives clients none.

## Code conventions

- C++23, no compiler extensions. Minimum toolchains: GCC 13, Clang 17 (libc++ 17), Apple Clang 15 (Xcode 15), MSVC 19.38 (VS 2022 17.8).
- Warnings: `-Wall -Wextra -Wpedantic` (MSVC: `/W4`), warnings are errors in CI.
- Prefer `std::span`, `std::array`, `std::unique_ptr`. No raw owning pointers.
- No exceptions across the core API boundary. Return `std::expected`.
- Allowed C++23 library features: `std::expected`, `std::to_underlying`, `std::unreachable`, `std::byteswap`, `std::ranges` additions supported by all minimum toolchains.
- Do not use: C++ modules, `std::flat_map`, `std::mdspan`, `std::generator`, `std::stacktrace`, `std::print` (uneven standard library support).
- Error type in `std::expected` is `std::string` or a dedicated error type.

### Naming
- Types (classes, structs, enums, aliases): `PascalCase`. Enumerators: `PascalCase`, use `enum class`.
- Functions and methods: `camelCase`.
- Variables, parameters and members: `snake_case`. Members have no prefix (no `m_`).
- Constants: `constexpr` in `snake_case`, no `UPPER_CASE`. `UPPER_CASE` only for macros.
- Files: `snake_case` with `.hpp` and `.cpp`, e.g. `deck_transport.hpp`.
- Namespaces follow the directory: `djw::core::engine`, `djw::player_ui::sdl`, ... (`-` in directory names becomes `_`). File-local helpers go into an anonymous namespace.

### Headers
- Include guards instead of `#pragma once`, built from the path: `DJW_CORE_ENGINE_DECK_HPP`, closed with `#endif // DJW_CORE_ENGINE_DECK_HPP`.
- Includes in this order, separated by a blank line: own header (in `.cpp`), project headers with path relative to the module root (`"engine/deck.hpp"`), then standard and third-party headers in `<>`.

### Formatting
Enforced by `.clang-format`. The clang-format version is pinned (see Static analysis and CI), because the output differs between versions:
- Based on LLVM style, 4 spaces indentation, no tabs, column limit 120.
- Allman braces (opening brace on its own line), also for namespaces, classes and functions.
- Namespace contents and access modifiers are indented; members below `public:`/`private:` are indented again.
- Consecutive assignments and declarations are aligned.
- Short functions on one line only when defined inline in the class. No single-line `if`.
- `&` and `*` bind to the name: `const Deck &deck`, `char *buffer`.

### Documentation
- Classes get a `/** ... */` block explaining purpose and behavior.
- Methods get a `///` line when the name alone is not enough.
- Comments explain why, not what.

### Structure
- Keep files small and focused. One class per file pair as a rule.
- No commented-out code in commits.

## Build

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
```

Presets to provide (create them in M0): `linux-debug`, `linux-release`, `linux-arm64-release`, `windows-debug`, `windows-release`, `macos-debug`, `macos-release`. Sanitizer presets: `linux-asan` (ASan + LeakSanitizer + UBSan) and `linux-tsan` (TSan), each with configure, build and test preset; the test presets set the sanitizer options (leak detection on, abort on the first error). Android, iOS and Web presets come with their milestones.

Build options:
- `DJW_PLAYER_SDL` (default ON): build the SDL3 player in `src/player-ui/sdl`
- `DJW_BUILD_TESTS` (default ON in debug)
- `DJW_WITH_FFMPEG` (default OFF)
- `DJW_WITH_DVS` (default OFF until M3)
- `DJW_WARNINGS_AS_ERRORS` (default OFF, ON in CI)
- `DJW_SANITIZE`: empty, `address` (ASan + LeakSanitizer + UBSan) or `thread` (TSan). Applies to the whole build including fetched dependencies.

## Tests

Tests live in `tests/<module>`, one doctest executable per module, so every module can be built and tested alone. All tests run with `ctest`.

### Unit tests
- Core logic gets unit tests (doctest).
- No mocking framework. Use hand-written fakes (`FakeClock`, `FakeAudioDevice`, `FakeMidiInput`, `MemoryFileSystem`, ...) and check the end state, not call sequences that break with every internal change.

### Integration tests
- Engine changes need an offline render test: run a command sequence through the command queue against the engine without an audio device, render to a buffer, compare against a golden file.
- Golden comparison uses a per-sample tolerance (maximum absolute deviation on normalized float samples, default `1e-5`, overridable per test), not bit-exact comparison, because float results differ between x86-64/ARM and between compilers.
- Golden files are generated on the reference platform (Linux x86-64, GCC) and committed together with the change that alters the output.
- Library tests run against a real SQLite database in a temporary directory.
- Controller mapping tests feed fake MIDI events through a TOML mapping and check the resulting commands.

### Architecture tests
- Target separation: core targets must not link SDL or any UI target. This is the hard boundary.
- An include check runs as a CTest test (CMake script, no extra dependency): `core/` must not include `player-ui/` or SDL headers, `player-ui/` may include from the core only `core/api/`.
- Real-time safety: in test builds `malloc`/`new` are replaced by counting versions. A test drives the audio callback with a typical command sequence and requires zero allocations inside it. Locks in the audio path are covered by TSan.

### Mutation tests
- They check that the tests actually detect changed behavior, not just execute the code. `tools/mutate.py` (Python, no dependency) changes one spot of a source file at a time (comparison, arithmetic and logical operators, constants, negations, conditions, removed statements), rebuilds, runs the tests of that module and lists the mutants no test noticed. Each surviving mutant is either a missing test or code that does not matter.
- Run it on a separate checkout (`git worktree`) with its own build folder, never on the working tree; `--shard i/n` splits the work over several checkouts. The usage is in the script's header.
- Scope for now: `src/core/`. A mutant that cannot matter (e.g. a log text) is marked in the source with a `mutation: skip` comment on its line.
- Not part of CI (too slow); run before a milestone is closed. A compiler-based tool (Mull) may follow later; like every dependency it must be approved first.

### Memory leaks
- Avoid leaks by construction: every resource has an owner (RAII). Heap memory through `std::unique_ptr` or containers, C library handles (miniaudio, SQLite, SDL, ...) in a small RAII wrapper with a deleter, never with manual `free`/`close` calls spread over the code.
- Leaks are checked, not assumed: LeakSanitizer runs with every ASan test run (`ASAN_OPTIONS=detect_leaks=1` set explicitly in the test environment). A reported leak fails the test.
- LeakSanitizer is only reliable on Linux (macOS on Apple Silicon and MSVC do not support it), so the Linux ASan run in CI is the authoritative leak check.
- Leaks inside third-party libraries are suppressed in a suppression file with a comment naming the library and the reason, never by disabling the check. The same holds for data races TSan reports inside third-party code (`tests/sanitizers/tsan.supp`, e.g. SQLite's lock-free WAL index header).
- Long-running paths (deck load/unload, adding/removing decks, library scans) get tests that repeat the operation many times, so a leak per operation shows up even if it is small.

### General
- Run tests before declaring a task done, including the sanitizer runs (ASan with LeakSanitizer, UBSan, TSan for threading code) before a milestone is closed.
- Debug builds must be able to run with ASan, UBSan and TSan.

## Static analysis and CI

- `.clang-tidy` in the repository root with `bugprone-*`, `modernize-*`, `performance-*`, `cppcoreguidelines-*`, `readability-identifier-naming` (enforces the naming rules above) and selected `misc-*` checks. Noisy checks are disabled there with a reason, not suppressed in code.
- clang-tidy runs on the sources in `src/`, not on `tests/` (the doctest macros trigger many checks), `third_party/` or fetched dependencies. Locally optional, in CI warnings are errors.
- clang-format and clang-tidy are pinned to one version in `.github/lint-requirements.txt` and installed with pip, so local runs and CI give the same result. Use the same version locally: `python3 -m venv .venv-lint && .venv-lint/bin/pip install -r .github/lint-requirements.txt`.
- Duplicated code is caught in review, there is no separate duplicate detection tool.
- CI (`.github/workflows/ci.yml`) runs for every push to `main` and every pull request, for now on Linux x86_64 only:
  - build and all tests with `linux-asan` (ASan + LeakSanitizer + UBSan) and `linux-tsan`, with warnings as errors and GCC 13 (the minimum)
  - Clang with libc++ is not in the CI; it is built on the Mac
  - clang-format check and clang-tidy
  - line coverage of `src/` with gcovr; the report is attached to the run. On `main` the coverage badge (an SVG made by `.github/scripts/coverage_badge.py`, no external service) is committed to the branch `badges`, from which the README shows it
- The CI does not start the SDL player, so leaks in system graphics drivers (seen locally with Mesa/EGL) do not show up there. A LeakSanitizer suppression file is added once a CI job runs the player.

## Workflow

- Small, focused commits. One logical change per commit.
- Commit messages in English, imperative mood (`Add deck command queue`, not `Added ...`).
- When a feature from FEATURES.md is completed, reference it in the commit or PR, so the spec can be tracked.
- If the spec is ambiguous or contradictory, ask instead of guessing.
- Do not refactor unrelated code while working on a task.

## Target hardware for testing

- Reference controller: Hercules DJControl Starlight (class-compliant USB MIDI + 4-channel audio: master on 1/2, headphones on 3/4).
- Performance baseline: Raspberry Pi 4 (4 GB). If something is too slow there, it is too slow.
