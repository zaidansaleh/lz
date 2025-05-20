#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TUPLE_LIST_CAPACITY 64
#define OUTPUT_CAPACITY 128

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

int tuple_list_push(TupleList *list, const Tuple *tuple) {
    if (list->length >= list->capacity) {
        return -1;
    }
    list->data[list->length++] = *tuple;
    return 0;
}

void tuple_list_free(TupleList *list) {
    free(list);
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

int main(void) {
    int retcode = 0;
    TupleList *tuples = NULL;

    const char *input = "abracadabrad";
    tuples = lz77_compress(input);
    if (!tuples) {
        fprintf(stderr, "error: lz77_compress failed\n");
        retcode = 1;
        goto cleanup;
    }

    char output[OUTPUT_CAPACITY];
    lz77_decompress(output, tuples);

    printf("Input: %s\n", input);
    printf("Tuples:\n");
    for (size_t i = 0; i < tuples->length; ++i) {
        const Tuple *tuple = &tuples->data[i];
        printf("(%d, %d, '%c')\n", tuple->offset, tuple->length, tuple->symbol);
    }
    printf("Output: %s\n", output);

cleanup:
    if (tuples) {
        tuple_list_free(tuples);
    }
    return retcode;
}

