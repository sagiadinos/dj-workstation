#!/usr/bin/env bash
# Builds the release binary: build/linux-release/src/player-ui/sdl/djw (with fonts/ next to it)
set -euo pipefail
cd "$(dirname "$0")/.."
cmake --preset linux-release
cmake --build --preset linux-release
