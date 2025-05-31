#ifndef LZ_H
#define LZ_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "string.h"

typedef union {
    struct {
        uint16_t offset;
        uint8_t length;
        uint8_t symbol;
    } lz77;
} CompressedRepr;

typedef enum {
    ALGO_LZ77,
} Algo;

typedef struct {
    Algo algo;
    size_t capacity;
    size_t length;
    CompressedRepr data[];
} CompressedReprList;

CompressedReprList *cr_list_new(Algo algo, size_t capacity);

void cr_list_free(CompressedReprList *list);

int cr_list_push(CompressedReprList *list, const CompressedRepr *cr);

void uint8_be_write(uint8_t *buf, uint8_t value);

void uint16_be_write(uint8_t *buf, uint16_t value);

uint8_t uint8_be_read(uint8_t *buf);

uint16_t uint16_be_read(uint8_t *buf);

const char *escape_char(char ch);

/* LZ77 */
int lz77_serialize(const CompressedRepr *cr, FILE *stream);
CompressedReprList *lz77_deserialize(FILE *stream);
CompressedReprList *lz77_compress(String *input);
String *lz77_decompress(const CompressedReprList *list);
void lz77_print(const CompressedRepr *cr, FILE *stream);

#endif // LZ_H

