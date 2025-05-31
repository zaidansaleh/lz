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

int lz_serialize(Algo algo, const void *compressed, FILE *stream) {
    int (*fn)(const void *, FILE *);
    switch (algo) {
    case ALGO_LZ77:
        fn = lz77_serialize;
        break;
    }
    return fn(compressed, stream);
}

void *lz_deserialize(Algo algo, FILE *stream) {
    switch (algo) {
    case ALGO_LZ77:
        return lz77_deserialize(stream);
    }
    return NULL;
}

void *lz_compress(Algo algo, const String *input) {
    switch (algo) {
    case ALGO_LZ77:
        return lz77_compress(input);
    }
    return NULL;
}

int lz_decompress(Algo algo, const void *compressed, FILE *stream) {
    String *(*fn)(const void *);
    switch (algo) {
    case ALGO_LZ77:
        fn = lz77_decompress;
        break;
    }

    String *buf = fn(compressed);
    if (!buf) {
        string_free(buf);
        return -1;
    }

    size_t written = fwrite(buf->data, 1, buf->length, stream);
    if (written != buf->length) {
        string_free(buf);
        return -1;
    }

    string_free(buf);
    return 0;
}

void lz_print(Algo algo, const void *compressed, FILE *stream) {
    void (*fn)(const void *, FILE *);
    switch (algo) {
    case ALGO_LZ77:
        fn = lz77_print;
        break;
    }
    fn(compressed, stream);
}

void lz_free(Algo algo, void *compressed) {
    void (*fn)(void *);
    switch (algo) {
        case ALGO_LZ77:
        fn = lz77_free;
        break;
    }
    fn(compressed);
}

typedef enum {
    MODE_COMPRESS,
    MODE_DECOMPRESS,
} Mode;

int main(int argc, const char *argv[]) {
    int retcode = 0;
    Algo algo = ALGO_LZ77;
    Mode mode = MODE_COMPRESS;
    bool show_help = false;
    int debug = 0;
    FILE *input_file = NULL;
    String *input = NULL;
    FILE *output_file = NULL;
    void *compressed = NULL;

    int arg_cursor = 0;
    const char *program_name = argv[arg_cursor++];

    for (int i = 0; i < argc; ++i) {
        const char *arg = argv[i];
        if (strcmp(arg, "-a") == 0 || strcmp(arg, "--algo") == 0) {
            const char *algo_str = argv[++i];
            if (strcmp(algo_str, "LZ77") == 0) {
                algo = ALGO_LZ77;
            }
        } else if (strcmp(arg, "-d") == 0 || strcmp(arg, "--decompress") == 0) {
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
            "  %-17s %s\n"
            "  %-17s %s\n",
            program_name,
            "-a, --algo", "The compression algorithm to use (default: LZ77)",
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
        compressed = lz_compress(ALGO_LZ77, input);
        if (!compressed) {
            fprintf(stderr, "error: lz_compress failed\n");
            retcode = 1;
            goto cleanup;
        }
        if (debug & DEBUG_COMPRESSED_REPR) {
            lz_print(ALGO_LZ77, compressed, stderr);
        }
        if (lz_serialize(ALGO_LZ77, compressed, output_file) < 0) {
            fprintf(stderr, "error: lz_serialize failed\n");
            retcode = 1;
            goto cleanup;
        }
        break;
    }
    case MODE_DECOMPRESS: {
        compressed = lz_deserialize(ALGO_LZ77, input_file);
        if (!compressed) {
            fprintf(stderr, "error: lz_deserialize failed\n");
            retcode = 1;
            goto cleanup;
        }
        if (debug & DEBUG_COMPRESSED_REPR) {
            lz_print(ALGO_LZ77, compressed, stderr);
        }
        if (lz_decompress(ALGO_LZ77, compressed, output_file) < 0) {
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
    if (compressed) {
        lz_free(ALGO_LZ77, compressed);
    }
    return retcode;
}

