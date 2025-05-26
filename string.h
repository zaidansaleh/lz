/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2025 Saleh Zaidan */

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

void string_free(String *s);

int string_push(String *s, char ch);

String *string_from_stream(FILE *stream);

#endif // STRING_H

