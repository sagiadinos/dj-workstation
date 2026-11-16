# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Niko Sagiadinos
#
# The CI of .github/workflows/ci.yml on the own machine: the same Ubuntu, the same compilers (gcc-13 13.3, gcc-14 14.2)
# and the lint tools in the versions of .github/lint-requirements.txt. Used by tools/ci-local.sh.
FROM ubuntu:24.04

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       ca-certificates git ninja-build libasound2-dev gcc-13 g++-13 gcc-14 g++-14 python3-venv \
    && rm -rf /var/lib/apt/lists/*

# CMake like on the GitHub runners (3.31), the lint tools pinned like in the CI
COPY lint-requirements.txt /tmp/lint-requirements.txt
RUN python3 -m venv /opt/lint \
    && /opt/lint/bin/pip install --quiet 'cmake==3.31.6' -r /tmp/lint-requirements.txt
ENV PATH=/opt/lint/bin:$PATH

# The source tree is mounted read-only and belongs to another user
RUN git config --system --add safe.directory '*'
