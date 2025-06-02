/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2025 Saleh Zaidan */

#ifndef LZ78_H
#define LZ78_H

#include <stdint.h>
#include <stdio.h>

#include "string.h"

int lz78_serialize(const void *compressed, FILE *stream);

void *lz78_deserialize(FILE *stream);

void *lz78_compress(const String *input);

String *lz78_decompress(const void *compressed);

void lz78_print(const void *compressed, FILE *stream);

void lz78_free(void *compressed);

#endif // LZ78_H

