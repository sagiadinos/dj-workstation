/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Copyright (C) 2026 Niko Sagiadinos */

/* miniaudio with its Ogg Vorbis decoder: stb_vorbis (public domain) must come before the implementation. */
#include "extras/stb_vorbis.c"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
