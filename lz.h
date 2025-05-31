/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2025 Saleh Zaidan */

#ifndef LZ_H
#define LZ_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "lz77.h"
#include "string.h"

typedef enum {
    ALGO_LZ77,
} Algo;

void uint8_be_write(uint8_t *buf, uint8_t value);

void uint16_be_write(uint8_t *buf, uint16_t value);

uint8_t uint8_be_read(uint8_t *buf);

uint16_t uint16_be_read(uint8_t *buf);

const char *escape_char(char ch);

#endif // LZ_H

