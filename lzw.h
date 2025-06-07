/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2025 Saleh Zaidan */

#ifndef LZW_H
#define LZW_H

#include <stdint.h>
#include <stdio.h>

#include "string.h"

int lzw_serialize(const void *compressed, FILE *stream);

void *lzw_deserialize(FILE *stream);

void *lzw_compress(const String *input);

String *lzw_decompress(const void *compressed);

void lzw_print(const void *compressed, FILE *stream);

void lzw_free(void *compressed);

#endif //  LZW_H

