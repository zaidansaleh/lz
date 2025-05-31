/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2025 Saleh Zaidan */
/* Version: 1.0.0 */

#ifndef STRING_H
#define STRING_H

#include <stddef.h>
#include <stdio.h>

typedef struct {
    size_t capacity;
    size_t length;
    char *data;
} String;

String *string_new();

String *string_copy(const String *s);

String *string_from_stream(FILE *stream);

String *string_from_cstr(const char *cstr);

int string_reserve(String *s, size_t capacity);

void string_clear(String *s);

void string_free(String *s);

int string_push(String *s, char ch);

#endif // STRING_H

