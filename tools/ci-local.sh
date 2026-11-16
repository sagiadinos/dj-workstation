#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Niko Sagiadinos
#
# The jobs of .github/workflows/ci.yml on the own machine, in a container with the same Ubuntu, compilers and tools,
# against what a commit would hold: the tracked files and the new ones not ignored, as they are now on disk.
#
#   tools/ci-local.sh [--clean] [asan] [tsan] [lint] [coverage]
#
# Without job names all four run, side by side. --clean throws away the build directories kept between runs (they
# keep the fetched dependencies, so a later run only builds what changed). The logs lie in
# ${XDG_CACHE_HOME:-~/.cache}/djw-ci-local/logs/. Exit status 0 only when every job passed.
set -euo pipefail

repo=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
cache=${XDG_CACHE_HOME:-$HOME/.cache}/djw-ci-local
image=djw-ci-local
sdl_flags=-DSDL_UNIX_CONSOLE_BUILD=ON

clean=0
jobs=()
for arg in "$@"; do
    case $arg in
        --clean) clean=1 ;;
        asan | tsan | lint | coverage) jobs+=("$arg") ;;
        *) echo "unknown argument: $arg" >&2; exit 2 ;;
    esac
done
((${#jobs[@]})) || jobs=(asan tsan lint coverage)

docker build --quiet --tag "$image" --file "$repo/tools/ci-local/Dockerfile" "$repo/.github" >/dev/null

# What a commit would hold: tracked files still on disk and new files not ignored. Times are kept, so the kept build
# directories only rebuild what changed.
snapshot=$cache/src
rm -rf "$snapshot"
mkdir -p "$snapshot/build" "$cache/logs"
git -C "$repo" ls-files -z --cached --others --exclude-standard \
    | (cd "$repo" && tar --null --files-from=- --ignore-failed-read -cf - 2>/dev/null) \
    | tar -xf - -C "$snapshot"
# A repository of its own, so that git ls-files in the jobs lists the same files as in the CI
git -C "$snapshot" init --quiet
git -C "$snapshot" -c core.safecrlf=false add --all --force

# The commands of each job, as in ci.yml
script_for() {
    case $1 in
        asan) cat <<EOF
export CC=gcc-13 CXX=g++-13
cmake --preset linux-asan -DDJW_WARNINGS_AS_ERRORS=ON $sdl_flags
cmake --build --preset linux-asan
ctest --preset linux-asan
EOF
        ;;
        # The kernel randomizes mmap with more bits than TSan's memory layout allows; setarch -R turns it off for the
        # tests (the CI lowers vm.mmap_rnd_bits instead, which a container cannot)
        tsan) cat <<EOF
export CC=gcc-13 CXX=g++-13
cmake --preset linux-tsan -DDJW_WARNINGS_AS_ERRORS=ON $sdl_flags
cmake --build --preset linux-tsan
setarch -R ctest --preset linux-tsan
EOF
        ;;
        lint) cat <<EOF
export CC=gcc-14 CXX=g++-14
git ls-files '*.cpp' '*.hpp' | xargs clang-format --dry-run --Werror
cmake --preset linux-debug $sdl_flags
git ls-files 'src/*.cpp' | xargs -P "\$(nproc)" -n 1 clang-tidy -p build/linux-debug --quiet --warnings-as-errors='*'
EOF
        ;;
        coverage) cat <<EOF
export CC=gcc-13 CXX=g++-13
cmake --preset linux-debug $sdl_flags "-DCMAKE_CXX_FLAGS=--coverage -fprofile-update=atomic" -DCMAKE_EXE_LINKER_FLAGS=--coverage
cmake --build --preset linux-debug
find build/linux-debug -name '*.gcda' -delete
ctest --preset linux-debug
mkdir -p build/coverage
gcovr --gcov-executable gcov-13 --root . --filter 'src/' --gcov-exclude-directories '.*/_deps/.*' \
      --gcov-ignore-parse-errors negative_hits.warn_once_per_file \
      --html-details build/coverage/index.html --txt build/coverage/summary.txt \
      --json-summary build/coverage/summary.json build/linux-debug
cat build/coverage/summary.txt
EOF
        ;;
    esac
}

declare -A pids
for job in "${jobs[@]}"; do
    volume=djw-ci-local-$job
    ((clean)) && docker volume rm --force "$volume" >/dev/null
    log=$cache/logs/$job.log
    echo "$job: running, log in $log"
    docker run --rm --init \
        --security-opt seccomp=unconfined \
        --volume "$snapshot:/src:ro" \
        --volume "$volume:/src/build" \
        --workdir /src \
        "$image" bash -c "
            set -euo pipefail
            set -x
            $(script_for "$job")
        " >"$log" 2>&1 &
    pids[$job]=$!
done

failed=()
for job in "${jobs[@]}"; do
    if wait "${pids[$job]}"; then
        echo "$job: passed"
    else
        echo "$job: FAILED"
        failed+=("$job")
    fi
done

for job in "${failed[@]}"; do
    echo
    echo "===== $job (last 40 lines of $cache/logs/$job.log)"
    tail -n 40 "$cache/logs/$job.log"
done
((${#failed[@]} == 0))
