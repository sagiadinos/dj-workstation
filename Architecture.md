# Architecture and Development Guidelines

Rules for everyone working on this repository.

## Project

djw (DJ-Workstation) is a lean, touch-first DJ application in C++23. It is a complete rewrite of the original Win32 DJ-Workstation, developed from 1998 to 2002. No code from the original is reused.

**`FEATURES.md` is the specification.** Read it before starting any task. Do not implement features that are not listed there, and never implement anything from its non-goals list. If a task seems to require something outside the spec, ask first.

## Current milestone

**M0 Skeleton** (see FEATURES.md, section 8). Work milestone by milestone. Do not start features of a later milestone unless explicitly asked.

## Hard rules

### Architecture
- `core/` must never include anything from `ui/` or SDL. The core builds and runs headless.
- The UI talks to the core only through the command queue and state snapshots in `core/api/`. No direct calls into engine internals.
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
- Dependencies are pulled via CMake `FetchContent` with pinned versions (tag or commit hash). Single-header libraries may be vendored in `third_party/` with their license file.
- Every dependency must be compatible with AGPLv3.

### Licensing
- Project license: AGPL-3.0-or-later.
- Every new source file starts with:
  ```cpp
  // SPDX-License-Identifier: AGPL-3.0-or-later
  // Copyright (C) 2026 Nikolaos Saghiadinos
  ```
- Files taken from third parties (e.g. xwax timecoder) keep their original headers and license unchanged.

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
- Namespaces follow the directory: `djw::core::engine`, `djw::ui`, ... File-local helpers go into an anonymous namespace.

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

Presets to provide (create them in M0): `linux-debug`, `linux-release`, `linux-arm64-release`, `windows-debug`, `windows-release`, `macos-debug`, `macos-release`. Android, iOS and Web presets come with their milestones.

Build options:
- `DJW_BUILD_UI` (default ON): build the SDL3 frontend
- `DJW_BUILD_TESTS` (default ON in debug)
- `DJW_WITH_FFMPEG` (default OFF)
- `DJW_WITH_DVS` (default OFF until M3)

## Tests

- Core logic gets unit tests (doctest).
- Engine changes need an offline render test: run a command sequence against the engine without an audio device, render to a buffer, compare against expected output.
- Run tests before declaring a task done.
- Debug builds should be able to run with ASan and UBSan, TSan for threading code.

## Workflow

- Small, focused commits. One logical change per commit.
- Commit messages in English, imperative mood (`Add deck command queue`, not `Added ...`).
- When a feature from FEATURES.md is completed, reference it in the commit or PR, so the spec can be tracked.
- If the spec is ambiguous or contradictory, ask instead of guessing.
- Do not refactor unrelated code while working on a task.

## Target hardware for testing

- Reference controller: Hercules DJControl Starlight (class-compliant USB MIDI + 4-channel audio: master on 1/2, headphones on 3/4).
- Performance baseline: Raspberry Pi 4 (4 GB). If something is too slow there, it is too slow.
