/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2025 Saleh Zaidan */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lz.h"
#include "string.h"

LZ77_TupleList *lz77_tuple_list_new(size_t capacity) {
    LZ77_TupleList *list = malloc(sizeof(LZ77_TupleList) + sizeof(LZ77_Tuple) * capacity);
    if (!list) {
        return NULL;
    }
    list->capacity = capacity;
    list->length = 0;
    memset(list->data, 0, sizeof(LZ77_Tuple) * capacity);
    return list;
}

int lz77_tuple_list_push(LZ77_TupleList *list, const LZ77_Tuple *tuple) {
    if (list->length >= list->capacity) {
        return -1;
    }
    list->data[list->length++] = *tuple;
    return 0;
}

int lz77_serialize(const void *compressed, FILE *stream) {
    const LZ77_TupleList *list = compressed;
    for (size_t i = 0; i < list->length; ++i) {
        const LZ77_Tuple *tuple = &list->data[i];
        uint8_t buf[4];
        uint16_be_write(buf, tuple->offset);
        uint8_be_write(buf + 2, tuple->length);
        uint8_be_write(buf + 3, tuple->symbol);
        size_t written = fwrite(buf, 1, sizeof(buf), stream);
        if (written != sizeof(buf)) {
            return -1;
        }
    }
    return 0;
}

void *lz77_deserialize(FILE *stream) {
    bool error = false;
    LZ77_TupleList *list = NULL;

    fseek(stream, 0, SEEK_END);
    long file_size = ftell(stream);
    rewind(stream);

    list = lz77_tuple_list_new(file_size);
    if (!list) {
        error = true;
        goto cleanup;
    }

    while (true) {
        uint8_t buf[4];
        size_t read = fread(buf, 1, sizeof(buf), stream);
        if (read != sizeof(buf)) {
            if (feof(stream)) {
                break;
            }
            error = true;
            goto cleanup;
        }
        LZ77_Tuple cr = {
            .offset = uint16_be_read(buf),
            .length = uint8_be_read(buf + 2),
            .symbol = uint8_be_read(buf + 3),
        };
        if (lz77_tuple_list_push(list, &cr) < 0) {
            error = true;
            goto cleanup;
        }
    }

    cleanup:
    if (error) {
        free(list);
        return NULL;
    }
    return list;
}

void *lz77_compress(const String *input) {
    bool error = false;
    LZ77_TupleList *list = NULL;

    list = lz77_tuple_list_new(input->length);
    if (!list) {
        error = true;
        goto cleanup;
    }

    for (size_t lookahead = 0; lookahead < input->length;) {
        size_t match_offset = 0;
        size_t match_length = 0;
        for (size_t start = lookahead; start-- > 0;) {
            size_t length = 0;
            while (start + length < input->length &&
                lookahead + length < input->length &&
                input->data[start + length] == input->data[lookahead + length]) {
                length += 1;
            }
            if (length > match_length) { 
                match_length = length;
                match_offset = lookahead - start;
            }
        }

        // WARNING: The null character ('\0') is used to indicate that there is no remaining symbol to emit.
        // This makes the implementation incompatible with binary input, where '\0' may be a valid data byte.
        LZ77_Tuple tuple = {
            .offset = match_offset,
            .length = match_length,
            .symbol = lookahead + match_length < input->length ? input->data[lookahead + match_length] : '\0',
        };
        if (lz77_tuple_list_push(list, &tuple) < 0) {
            error = true;
            goto cleanup;
        }
        lookahead += match_length + 1;
    }

    cleanup:
    if (error) {
        free(list);
        return NULL;
    }
    return list;
}

String *lz77_decompress(const void *compressed) {
    const LZ77_TupleList *list = compressed;
    bool error = false;
    String *buf = NULL;

    buf = string_new();
    if (!buf) {
        error = true;
        goto cleanup;
    }
    for (size_t i = 0; i < list->length; ++i) {
        const LZ77_Tuple *tuple = &list->data[i];
        for (size_t j = 0; j < tuple->length; ++j) {
            if (string_push(buf, buf->data[buf->length - tuple->offset]) < 0) {
                error = true;
                goto cleanup;
            }
        }
        if (tuple->symbol != '\0' && string_push(buf, tuple->symbol) < 0) {
            error = true;
            goto cleanup;
        }
    }

cleanup:
    if (error) {
        string_free(buf);
        return NULL;
    }
    return buf;
}

void lz77_print(const void *compressed, FILE *stream) {
    const LZ77_Tuple *tuple = compressed;
    fprintf(stream, "(%d, %d, '%s')\n", tuple->offset, tuple->length, escape_char(tuple->symbol));
}

void lz77_free(void *compressed) {
    free(compressed);
}

