/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2025 Saleh Zaidan */

#ifndef LZ77_H
#define LZ77_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "string.h"

typedef struct {
    uint16_t offset;
    uint8_t length;
    uint8_t symbol;
} LZ77_Tuple;

typedef struct {
    size_t capacity;
    size_t length;
    LZ77_Tuple data[];
} LZ77_TupleList;

int lz77_serialize(const void *compressed, FILE *stream);

void *lz77_deserialize(FILE *stream);

void *lz77_compress(const String *input);

String *lz77_decompress(const void *compressed);

void lz77_print(const void *compressed, FILE *stream);

void lz77_free(void *compressed);

#endif // LZ77_H

