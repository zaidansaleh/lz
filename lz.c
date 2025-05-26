/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2025 Saleh Zaidan */

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lz.h"
#include "string.h"

#define DEBUG_COMPRESSED_REPR (1 << 0)

const char *escape_char(char ch) {
    static char escape_char_buf[2];
    if (isprint(ch)) {
        escape_char_buf[0] = ch;
        escape_char_buf[1] = '\0';
        return escape_char_buf;
    }
    switch (ch) {
    case '\n': return "\\n";
    case '\t': return "\\t";
    case '\r': return "\\r";
    case '\b': return "\\b";
    case '\f': return "\\f";
    case '\v': return "\\v";
    case '\\': return "\\\\";
    case '\'': return "\\'";
    case '\"': return "\\\"";
    case '\0': return "\\0";
    }
    return NULL;
}

void uint8_be_write(uint8_t *buf, uint8_t value) {
    buf[0] = value & 0xFF;
}

void uint16_be_write(uint8_t *buf, uint16_t value) {
    buf[0] = (value >> 8) & 0xFF;
    buf[1] = value & 0xFF;
}

uint8_t uint8_be_read(uint8_t *buf) {
    return buf[0];
}

uint16_t uint16_be_read(uint8_t *buf) {
    return buf[0] << 8 | buf[1];
}

CompressedReprList *cr_list_new(Algo algo, size_t capacity) {
    CompressedReprList *list = malloc(sizeof(CompressedReprList) + sizeof(CompressedRepr) * capacity);
    if (!list) {
        return NULL;
    }
    list->algo = algo;
    list->capacity = capacity;
    list->length = 0;
    memset(list->data, 0, sizeof(CompressedRepr) * capacity);
    return list;
}

void cr_list_free(CompressedReprList *list) {
    free(list);
}

int cr_list_push(CompressedReprList *list, const CompressedRepr *cr) {
    if (list->length >= list->capacity) {
        return -1;
    }
    list->data[list->length++] = *cr;
    return 0;
}

int lz_serialize(const CompressedReprList *list, FILE *stream) {
    int (*fn)(const CompressedRepr *, FILE *);
    switch (list->algo) {
    case ALGO_LZ77:
        fn = lz77_serialize;
        break;
    }
    for (size_t i = 0; i < list->length; ++i) {
        const CompressedRepr *cr = &list->data[i];
        if (fn(cr, stream) < 0) {
            return -1;
        }
    }
    return 0;
}

CompressedReprList *lz_deserialize(Algo algo, FILE *stream) {
    switch (algo) {
    case ALGO_LZ77:
        return lz77_deserialize(stream);
    }
    return NULL;
}

CompressedReprList *lz_compress(Algo algo, String *input) {
    switch (algo) {
    case ALGO_LZ77:
        return lz77_compress(input);
    }
    return NULL;
}

int lz_decompress(const CompressedReprList *list, FILE *stream) {
    int (*fn)(const CompressedRepr *, String *);
    switch (list->algo) {
    case ALGO_LZ77:
        fn = lz77_decompress;
        break;
    }
    String *buf = string_new();
    for (size_t i = 0; i < list->length; ++i) {
        const CompressedRepr *cr = &list->data[i];
        if (fn(cr, buf) < 0) {
            return -1;
        }
    }
    size_t written = fwrite(buf->data, 1, buf->length, stream);
    if (written != buf->length) {
        return -1;
    }
    string_free(buf);
    return 0;
}

void lz_print(const CompressedReprList *list, FILE *stream) {
    void (*fn)(const CompressedRepr *, FILE *);
    switch (list->algo) {
    case ALGO_LZ77:
        fn = lz77_print;
        break;
    }
    for (size_t i = 0; i < list->length; ++i) {
        const CompressedRepr *cr = &list->data[i];
        fn(cr, stream);
    }
}

typedef enum {
    MODE_COMPRESS,
    MODE_DECOMPRESS,
} Mode;

int main(int argc, const char *argv[]) {
    int retcode = 0;
    Mode mode = MODE_COMPRESS;
    bool show_help = false;
    int debug = 0;
    FILE *input_file = NULL;
    String *input = NULL;
    FILE *output_file = NULL;
    CompressedReprList *cr_list = NULL;

    int arg_cursor = 0;
    const char *program_name = argv[arg_cursor++];

    for (int i = 0; i < argc; ++i) {
        const char *arg = argv[i];
        if (strcmp(arg, "-d") == 0 || strcmp(arg, "--decompress") == 0) {
            mode = MODE_DECOMPRESS;
            arg_cursor += 1;
        } else if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            show_help = true;
            arg_cursor += 1;
        } else if (strncmp(arg, "--debug-", 8) == 0) {
            const char *debug_level = arg + 8;
            if (strcmp(debug_level, "cr") == 0) {
                debug |= DEBUG_COMPRESSED_REPR;
                arg_cursor += 1;
            }
        }
    }

    if (show_help) {
        printf(
            "Usage: %s [options] [input] [output]\n"
            "Compress input file using Lempel-Ziv algorithms.\n"
            "\n"
            "Options:\n"
            "  %-17s %s\n"
            "  %-17s %s\n"
            "  %-17s %s\n",
            program_name,
            "-d, --decompress", "Decompress input instead of compressing",
            "--debug-cr", "Print the compressed representation to stderr",
            "-h, --help", "Display this help message"
        );
        goto cleanup;
    }

    if (arg_cursor >= argc) {
        input_file = stdin;
    } else if (argv[arg_cursor][0] == '-') {
        arg_cursor++;
        input_file = stdin;
    } else {
        const char *input_pathname = argv[arg_cursor++];
        input_file = fopen(input_pathname, "r");
        if (!input_file) {
            fprintf(stderr, "error: input file '%s' open failed\n", input_pathname);
            retcode = 1;
            goto cleanup;
        }
    }

    if (arg_cursor >= argc) {
        output_file = stdout;
    } else {
        const char *output_pathname = argv[arg_cursor++];
        output_file = fopen(output_pathname, "w");
        if (!output_file) {
            fprintf(stderr, "error: output file '%s' open failed\n", output_pathname);
            retcode = 1;
            goto cleanup;
        }
    }

    input = string_from_stream(input_file);
    if (!input) {
        fprintf(stderr, "error: string_from_stream failed\n");
        retcode = 1;
        goto cleanup;
    }

    switch (mode) {
    case MODE_COMPRESS: {
        cr_list = lz_compress(ALGO_LZ77, input);
        if (!cr_list) {
            fprintf(stderr, "error: lz_compress failed\n");
            retcode = 1;
            goto cleanup;
        }
        if (debug & DEBUG_COMPRESSED_REPR) {
            lz_print(cr_list, stderr);
        }
        if (lz_serialize(cr_list, output_file) < 0) {
            fprintf(stderr, "error: lz_serialize failed\n");
            retcode = 1;
            goto cleanup;
        }
        break;
    }
    case MODE_DECOMPRESS: {
        cr_list = lz_deserialize(ALGO_LZ77, input_file);
        if (!cr_list) {
            fprintf(stderr, "error: lz_deserialize failed\n");
            retcode = 1;
            goto cleanup;
        }
        if (debug & DEBUG_COMPRESSED_REPR) {
            lz_print(cr_list, stderr);
        }
        if (lz_decompress(cr_list, output_file) < 0) {
            fprintf(stderr, "error: lz_decompress failed\n");
            retcode = 1;
            goto cleanup;
        }
        break;
    }
    }

cleanup:
    if (input_file) {
        fclose(input_file);
    }
    if (input) {
        string_free(input);
    }
    if (output_file) {
        fclose(output_file);
    }
    if (cr_list) {
        cr_list_free(cr_list);
    }
    return retcode;
}

