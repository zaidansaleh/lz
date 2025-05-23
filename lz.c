#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG_TUPLE (1 << 0)

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

typedef struct {
    size_t capacity;
    size_t length;
    char *data;
} String;

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

int string_move(String *s, const char *src, size_t n) {
    char *buf = malloc(sizeof(char) * n);
    if (!buf) {
        return -1;
    }
    for (size_t i = 0; i < n; ++i) {
        buf[i] = src[i];
    }
    for (size_t i = 0; i < n; ++i) {
        char ch = buf[i];
        if (string_push(s, ch) < 0) {
            free(buf);
            return -1;
        }
    }
    free(buf);
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

typedef struct {
    uint16_t offset;
    uint8_t length;
    uint8_t symbol;
} Tuple;

void tuple_print(const Tuple *tuple, FILE *stream) {
    fprintf(stream, "(%d, %d, '%s')\n", tuple->offset, tuple->length, escape_char(tuple->symbol));
}

typedef struct {
    size_t capacity;
    size_t length;
    Tuple data[];
} TupleList;

TupleList *tuple_list_new(size_t capacity) {
    TupleList *list = malloc(sizeof(TupleList) + sizeof(Tuple) * capacity);
    if (!list) {
        return NULL;
    }
    list->capacity = capacity;
    list->length = 0;
    memset(list->data, 0, sizeof(Tuple) * capacity);
    return list;
}

void tuple_list_free(TupleList *list) {
    free(list);
}

int tuple_list_push(TupleList *list, const Tuple *tuple) {
    if (list->length >= list->capacity) {
        return -1;
    }
    list->data[list->length++] = *tuple;
    return 0;
}

int tuple_list_serialize(const TupleList *list, FILE *stream) {
    for (size_t i = 0; i < list->length; ++i) {
        const Tuple *tuple = &list->data[i];
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

TupleList *tuple_list_deserialize(FILE *stream) {
    bool error = false;
    TupleList *list = NULL;

    fseek(stream, 0, SEEK_END);
    long file_size = ftell(stream);
    rewind(stream);

    list = tuple_list_new(file_size);
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
        Tuple tuple = {
            .offset = uint16_be_read(buf),
            .length = uint8_be_read(buf + 2),
            .symbol = uint8_be_read(buf + 3),
        };
        if (tuple_list_push(list, &tuple) < 0) {
            error = true;
            goto cleanup;
        }
    }

cleanup:
    if (error) {
        tuple_list_free(list);
        return NULL;
    }
    return list;
}

TupleList *lz77_compress(String *input) {
    bool error = false;
    TupleList *tuples = NULL;

    tuples = tuple_list_new(input->length);
    if (!tuples) {
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

        Tuple tuple = {
            .offset = match_offset,
            .length = match_length,
            .symbol = lookahead + match_length < input->length ? input->data[lookahead + match_length] : '\0',
        };
        if (tuple_list_push(tuples, &tuple) < 0) {
            error = true;
            goto cleanup;
        }
        lookahead += match_length + 1;
    }

cleanup:
    if (error) {
        tuple_list_free(tuples);
        return NULL;
    }
    return tuples;
}

void tuple_list_pprint(const TupleList *list, FILE *stream) {
    for (size_t i = 0; i < list->length; ++i) {
        const Tuple *tuple = &list->data[i];
        tuple_print(tuple, stream);
    }
}

int lz77_decompress(const TupleList *list, FILE *stream) {
    String *buf = string_new();
    for (size_t i = 0; i < list->length; ++i) {
        const Tuple *tuple = &list->data[i];
        if (string_move(buf, buf->data + buf->length - tuple->offset, tuple->length) < 0) {
            return -1;
        }
        if (string_push(buf, tuple->symbol) < 0) {
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
    TupleList *tuples = NULL;

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
            if (strcmp(debug_level, "tuple") == 0) {
                debug |= DEBUG_TUPLE;
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
            "--debug-tuple", "Print the tuple to stderr",
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
        tuples = lz77_compress(input);
        if (!tuples) {
            fprintf(stderr, "error: lz77_compress failed\n");
            retcode = 1;
            goto cleanup;
        }
        if (debug & DEBUG_TUPLE) {
            tuple_list_pprint(tuples, stderr);
        }
        if (tuple_list_serialize(tuples, output_file) < 0) {
            fprintf(stderr, "error: tuple_list_serialize failed\n");
            retcode = 1;
            goto cleanup;
        }
        break;
    }
    case MODE_DECOMPRESS: {
        tuples = tuple_list_deserialize(input_file);
        if (!tuples) {
            fprintf(stderr, "error: tuple_list_deserialize failed\n");
            retcode = 1;
            goto cleanup;
        }
        if (debug & DEBUG_TUPLE) {
            tuple_list_pprint(tuples, stderr);
        }
        if (lz77_decompress(tuples, output_file) < 0) {
            fprintf(stderr, "error: lz77_decompress failed\n");
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
    if (tuples) {
        tuple_list_free(tuples);
    }
    return retcode;
}

