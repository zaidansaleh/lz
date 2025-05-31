/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2025 Saleh Zaidan */
/* Version: 1.0.0 */

#include <stdlib.h>
#include <string.h>

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
        string_free(s);
        return NULL;
    }
    data[s->length] = '\0';
    s->data = data;
    return s;
#undef STRING_DEFAULT_CAPACITY
}

String *string_copy(const String *s) {
    String *s_copy = string_new();
    if (!s_copy) {
        return NULL;
    }
    // +1 to reserve space for the null terminator.
    if (string_reserve(s_copy, s->length + 1) < 0) {
        string_free(s_copy);
        return NULL;
    }
    memcpy(s_copy->data, s->data, s->length);
    s_copy->length = s->length;
    s_copy->data[s_copy->length] = '\0';
    return s_copy;
}

String *string_from_stream(FILE *stream) {
    String *s = string_new();
    if (!s) {
        return NULL;
    }
    int ch;
    while ((ch = fgetc(stream)) != EOF) {
        if (string_push(s, ch) < 0) {
            string_free(s);
            return NULL;
        }
    }
    return s;
}

String *string_from_cstr(const char *cstr) {
    size_t cstr_len = strlen(cstr);
    String *s = string_new();
    if (!s) {
        return NULL;
    }
    // +1 to reserve space for the null terminator.
    if (string_reserve(s, cstr_len + 1) < 0) {
        string_free(s);
        return NULL;
    }
    memcpy(s->data, cstr, cstr_len);
    s->length = cstr_len;
    s->data[s->length] = '\0';
    return s;
}

int string_reserve(String *s, size_t capacity) {
    if (s->capacity >= capacity) {
        return 0;
    }
    char *data_new = realloc(s->data, sizeof(char) * capacity);
    if (!data_new) {
        return -1;
    }
    s->capacity = capacity;
    s->data = data_new;
    return 0;
}

void string_free(String *s) {
    free(s->data);
    free(s);
}

void string_clear(String *s) {
    s->length = 0;
    s->data[s->length] = '\0';
}

int string_push(String *s, char ch) {
    // +1 to reserve space for the null terminator.
    if (s->length + 1 >= s->capacity) {
        if (string_reserve(s, s->capacity * 2) < 0) {
            return -1;
        }
    }
    s->data[s->length++] = ch;
    s->data[s->length] = '\0';
    return 0;
}

