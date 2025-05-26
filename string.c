/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2025 Saleh Zaidan */

#include <stdlib.h>

#include "string.h"

String *string_new() {
#define STRING_DEFAULT_CAPACITY 8
    String *s = malloc(sizeof(String));
    if (!s) {
        return NULL;
    }
    s->capacity = STRING_DEFAULT_CAPACITY;
    s->length = 0;
    char *data = malloc(sizeof(char) * STRING_DEFAULT_CAPACITY);
    if (!data) {
        return NULL;
    }
    data[s->length] = '\0';
    s->data = data;
    return s;
#undef STRING_DEFAULT_CAPACITY
}

void string_free(String *s) {
    free(s->data);
    free(s);
}

int string_push(String *s, char ch) {
    // Make sure length + 1 bytes are available (actual length plus null terminator).
    if (s->length + 1 >= s->capacity) {
        size_t capacity_new = s->capacity * 2;
        char *data_new = realloc(s->data, sizeof(char) * capacity_new);
        if (!data_new) {
            return -1;
        }
        s->capacity = capacity_new;
        s->data = data_new;
    }
    s->data[s->length++] = ch;
    s->data[s->length] = '\0';
    return 0;
}

String *string_from_stream(FILE *stream) {
    String *s = string_new();
    int ch;
    while ((ch = fgetc(stream)) != EOF) {
        if (string_push(s, ch) < 0) {
            string_free(s);
            return NULL;
        }
    }
    return s;
}

