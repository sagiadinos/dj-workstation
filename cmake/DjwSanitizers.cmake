# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Niko Sagiadinos

# Sanitizer builds, selected with DJW_SANITIZE:
# - address: AddressSanitizer with LeakSanitizer and UndefinedBehaviorSanitizer
# - thread:  ThreadSanitizer
#
# The flags are set for the whole build, including fetched dependencies such as doctest: TSan reports false races
# when only part of the program is instrumented.

set(DJW_SANITIZE "" CACHE STRING "Sanitizer build: empty, address (ASan + LSan + UBSan) or thread (TSan)")
set_property(CACHE DJW_SANITIZE PROPERTY STRINGS "" address thread)

if(DJW_SANITIZE STREQUAL "")
    return()
endif()

if(MSVC)
    if(NOT DJW_SANITIZE STREQUAL "address")
        message(FATAL_ERROR "MSVC supports only DJW_SANITIZE=address")
    endif()
    add_compile_options(/fsanitize=address)
    return()
endif()

if(DJW_SANITIZE STREQUAL "address")
    # UBSan findings stay recoverable in the build: with -fno-sanitize-recover the program aborts even on a finding
    # that tests/sanitizers/ubsan.supp suppresses. The linux-asan test preset stops at the first finding instead
    # (UBSAN_OPTIONS halt_on_error=1), so every finding that is not suppressed still fails.
    set(_djw_sanitize_flags -fsanitize=address,undefined)
elseif(DJW_SANITIZE STREQUAL "thread")
    set(_djw_sanitize_flags -fsanitize=thread)
else()
    message(FATAL_ERROR "Unknown DJW_SANITIZE value '${DJW_SANITIZE}', use address or thread")
endif()

add_compile_options(${_djw_sanitize_flags} -fno-omit-frame-pointer -g)
add_link_options(${_djw_sanitize_flags})
