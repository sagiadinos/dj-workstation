# Architecture and Development Guidelines

Rules for everyone working on this repository.

## Project

djw (DJ-Workstation) is a lean, touch-first DJ application in C++23. It is a complete rewrite of the original Win32 DJ-Workstation, developed from 1998 to 2002. No code from the original is reused.

**`FEATURES.md` is the specification.** Read it before starting any task. Do not implement features that are not listed there, and never implement anything from its non-goals list. If a task seems to require something outside the spec, ask first.

## Current milestone

**M0 Skeleton** (see FEATURES.md, section 8). Work milestone by milestone. Do not start features of a later milestone unless explicitly asked.

## Hard rules

### Architecture
- `core/` must never include anything from `player-ui/` or SDL. The core builds and runs headless.
- The frontends in `player-ui/` talk to the core only through `core/api/`: setup (adding decks, starting audio) through the `api::Core` interface, while running only through the command queue and state snapshots. No direct calls into engine internals.
- Platform-specific code lives in `platform/` or behind an interface, never scattered with `#ifdef` across the core.

### Audio thread
The audio callback and everything it calls must be real-time safe:
- no heap allocation or deallocation (`new`, `delete`, `malloc`, growing `std::vector`, `std::string`, `std::function` with captures)
- no locks (`std::mutex`, condition variables), only atomics and lock-free queues
- no I/O, no logging, no syscalls
- no exceptions
- bounded execution time

Mark audio-thread functions with a comment `// [audio-thread]` so they are easy to audit. When in doubt whether something is RT-safe, it is not.

### Dependencies
- Only use libraries listed in FEATURES.md section 6.
- Ask before adding any new dependency.
- Dependencies are declared via CMake `FetchContent` with pinned versions (tag or commit hash) and `FIND_PACKAGE_ARGS`: an installed system or vcpkg package is used if found, otherwise the pinned version is downloaded. Exception: libraries without a CMake package config (miniaudio) are always downloaded and built from their sources by our own CMake target. Single-header libraries may be vendored in `third_party/` with their license file.
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
Enforced by `.clang-format` (clang-format 19 or newer):
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
- Planned for later, once the core modules have solid unit test coverage. They check that the tests actually detect changed behavior, not just execute the code. The tool is chosen then and must be approved like any other dependency.

### Memory leaks
- Avoid leaks by construction: every resource has an owner (RAII). Heap memory through `std::unique_ptr` or containers, C library handles (miniaudio, SQLite, SDL, ...) in a small RAII wrapper with a deleter, never with manual `free`/`close` calls spread over the code.
- Leaks are checked, not assumed: LeakSanitizer runs with every ASan test run (`ASAN_OPTIONS=detect_leaks=1` set explicitly in the test environment). A reported leak fails the test.
- LeakSanitizer is only reliable on Linux (macOS on Apple Silicon and MSVC do not support it), so the Linux ASan run in CI is the authoritative leak check.
- Leaks inside third-party libraries are suppressed in a suppression file with a comment naming the library and the reason, never by disabling the check.
- Long-running paths (deck load/unload, adding/removing decks, library scans) get tests that repeat the operation many times, so a leak per operation shows up even if it is small.

### General
- Run tests before declaring a task done, including the sanitizer runs (ASan with LeakSanitizer, UBSan, TSan for threading code) before a milestone is closed.
- Debug builds must be able to run with ASan, UBSan and TSan.

## Static analysis and CI

- `.clang-tidy` in the repository root with `bugprone-*`, `modernize-*`, `performance-*`, `cppcoreguidelines-*`, `readability-identifier-naming` (enforces the naming rules above) and selected `misc-*` checks. Noisy checks are disabled there with a reason, not suppressed in code.
- clang-tidy runs on project targets only, not on `third_party/` or fetched dependencies. Locally optional, in CI warnings are errors.
- Duplicated code is caught in review, there is no separate duplicate detection tool.
- CI runs for every push and pull request: build with warnings as errors, `ctest`, ASan/UBSan with LeakSanitizer and TSan runs, clang-format check, clang-tidy, and test coverage.

## Workflow

- Small, focused commits. One logical change per commit.
- Commit messages in English, imperative mood (`Add deck command queue`, not `Added ...`).
- When a feature from FEATURES.md is completed, reference it in the commit or PR, so the spec can be tracked.
- If the spec is ambiguous or contradictory, ask instead of guessing.
- Do not refactor unrelated code while working on a task.

## Target hardware for testing

- Reference controller: Hercules DJControl Starlight (class-compliant USB MIDI + 4-channel audio: master on 1/2, headphones on 3/4).
- Performance baseline: Raspberry Pi 4 (4 GB). If something is too slow there, it is too slow.
