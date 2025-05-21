#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TUPLE_LIST_CAPACITY 64
#define OUTPUT_CAPACITY 128

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
    uint16_t offset;
    uint8_t length;
    uint8_t symbol;
} Tuple;

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

    list = tuple_list_new(TUPLE_LIST_CAPACITY);
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

TupleList *lz77_compress(const char *input) {
    bool error = false;
    TupleList *output = NULL;

    size_t input_length = strlen(input);
    output = tuple_list_new(TUPLE_LIST_CAPACITY);
    if (!output) {
        error = true;
        goto cleanup;
    }

    size_t position = 0;
    while (position < input_length) {
        uint16_t max_offset = 0;
        uint8_t max_length = 0;
        for (size_t i = position; i-- > 0;) {
            uint8_t length = 0;
            while (i + length < input_length &&
                   position + length < input_length &&
                   input[i + length] == input[position + length]) {
                length += 1;
                if (length > max_length) { 
                    max_length = length;
                    max_offset = position - i;
                }
            }
        }

        Tuple tuple = {
            .offset = max_offset,
            .length = max_length,
            .symbol = position + max_length < input_length ? input[position + max_length] : '\0',
        };
        if (tuple_list_push(output, &tuple) < 0) {
            error = true;
            goto cleanup;
        }
        position += max_length + 1;
    }

cleanup:
    if (error) {
        tuple_list_free(output);
        return NULL;
    }
    return output;
}

void lz77_decompress(char *output, const TupleList *list) {
    uint8_t length = 0;
    for (size_t i = 0; i < list->length; ++i) {
        const Tuple *tuple = &list->data[i];
        memcpy(output + length, output + length - tuple->offset, tuple->length);
        length += tuple->length;
        output[length++] = tuple->symbol;
    }
    output[length] = '\0';
}

typedef enum {
    MODE_COMPRESS,
    MODE_DECOMPRESS,
} Mode;

int main(void) {
    int retcode = 0;
    Mode mode = MODE_COMPRESS;
    TupleList *tuples = NULL;

    switch (mode) {
    case MODE_COMPRESS: {
        const char *input = "abracadabrad";
        tuples = lz77_compress(input);
        if (!tuples) {
            fprintf(stderr, "error: lz77_compress failed\n");
            retcode = 1;
            goto cleanup;
        }
        if (tuple_list_serialize(tuples, stdout) < 0) {
            fprintf(stderr, "error: tuple_list_serialize failed\n");
            retcode = 1;
            goto cleanup;
        }
        break;
    }
    case MODE_DECOMPRESS: {
        char output[OUTPUT_CAPACITY];
        lz77_decompress(output, tuples);
        break;
    }
    }

cleanup:
    if (tuples) {
        tuple_list_free(tuples);
    }
    return retcode;
}

