/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Copyright (C) 2026 Niko Sagiadinos */

/* miniaudio with its Ogg Vorbis decoder: the stb_vorbis (public domain) header must come before the implementation,
   its implementation after it. Its macros (L, C, R, ...) would otherwise break winnt.h on Windows. */
#define STB_VORBIS_HEADER_ONLY
#include "extras/stb_vorbis.c"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#undef STB_VORBIS_HEADER_ONLY
#include "extras/stb_vorbis.c"