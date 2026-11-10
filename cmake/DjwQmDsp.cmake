# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Niko Sagiadinos

# QM-DSP (Queen Mary, University of London): onset detection function and tempo tracking for the BPM analysis.
# GPL-2.0-or-later, which is compatible with our AGPL-3.0-or-later. Downloaded and built static from the few files
# the tempo tracker needs (no LAPACK); the target is djw::qmdsp. Third-party code, so its warnings are off.

include(FetchContent)
FetchContent_Declare(qmdsp
    GIT_REPOSITORY https://github.com/c4dm/qm-dsp.git
    GIT_TAG        e34a3cc
    SYSTEM
)
FetchContent_MakeAvailable(qmdsp)

add_library(djw_qmdsp STATIC
    ${qmdsp_SOURCE_DIR}/dsp/onsets/DetectionFunction.cpp
    ${qmdsp_SOURCE_DIR}/dsp/phasevocoder/PhaseVocoder.cpp
    ${qmdsp_SOURCE_DIR}/dsp/tempotracking/TempoTrackV2.cpp
    ${qmdsp_SOURCE_DIR}/dsp/transforms/FFT.cpp
    ${qmdsp_SOURCE_DIR}/maths/MathUtilities.cpp
    ${qmdsp_SOURCE_DIR}/ext/kissfft/kiss_fft.c
    ${qmdsp_SOURCE_DIR}/ext/kissfft/tools/kiss_fftr.c
)
target_include_directories(djw_qmdsp SYSTEM PUBLIC ${qmdsp_SOURCE_DIR})
target_include_directories(djw_qmdsp SYSTEM PRIVATE
    ${qmdsp_SOURCE_DIR}/ext/kissfft
    ${qmdsp_SOURCE_DIR}/ext/kissfft/tools
)
target_compile_definitions(djw_qmdsp PRIVATE kiss_fft_scalar=double)
if(MSVC)
    target_compile_options(djw_qmdsp PRIVATE /w)
else()
    # -O2 even in debug builds: the analysis runs over whole tracks and would crawl otherwise
    target_compile_options(djw_qmdsp PRIVATE -w -O2)
endif()
set_target_properties(djw_qmdsp PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(djw::qmdsp ALIAS djw_qmdsp)
