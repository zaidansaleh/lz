#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "lz.h"
#include "string.h"

int lz77_serialize(const CompressedRepr *cr, FILE *stream) {
    uint8_t buf[4];
    uint16_be_write(buf, cr->lz77.offset);
    uint8_be_write(buf + 2, cr->lz77.length);
    uint8_be_write(buf + 3, cr->lz77.symbol);
    size_t written = fwrite(buf, 1, sizeof(buf), stream);
    if (written != sizeof(buf)) {
        return -1;
    }
    return 0;
}

CompressedReprList *lz77_deserialize(FILE *stream) {
    bool error = false;
    CompressedReprList *list = NULL;

    fseek(stream, 0, SEEK_END);
    long file_size = ftell(stream);
    rewind(stream);

    list = cr_list_new(ALGO_LZ77, file_size);
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
        CompressedRepr cr = {
            .lz77 = {
                .offset = uint16_be_read(buf),
                .length = uint8_be_read(buf + 2),
                .symbol = uint8_be_read(buf + 3),
            },
        };
        if (cr_list_push(list, &cr) < 0) {
            error = true;
            goto cleanup;
        }
    }

    cleanup:
    if (error) {
        cr_list_free(list);
        return NULL;
    }
    return list;
}

CompressedReprList *lz77_compress(String *input) {
    bool error = false;
    CompressedReprList *list = NULL;

    list = cr_list_new(ALGO_LZ77, input->length);
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
        CompressedRepr cr = {
            .lz77 = {
                .offset = match_offset,
                .length = match_length,
                .symbol = lookahead + match_length < input->length ? input->data[lookahead + match_length] : '\0',
            }
        };
        if (cr_list_push(list, &cr) < 0) {
            error = true;
            goto cleanup;
        }
        lookahead += match_length + 1;
    }

    cleanup:
    if (error) {
        cr_list_free(list);
        return NULL;
    }
    return list;
}

String *lz77_decompress(const CompressedReprList *list) {
    bool error = false;
    String *buf = NULL;

    buf = string_new();
    if (!buf) {
        error = true;
        goto cleanup;
    }
    for (size_t i = 0; i < list->length; ++i) {
        const CompressedRepr *cr = &list->data[i];
        for (size_t j = 0; j < cr->lz77.length; ++j) {
            if (string_push(buf, buf->data[buf->length - cr->lz77.offset]) < 0) {
                error = true;
                goto cleanup;
            }
        }
        if (cr->lz77.symbol != '\0' && string_push(buf, cr->lz77.symbol) < 0) {
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

void lz77_print(const CompressedRepr *cr, FILE *stream) {
    fprintf(stream, "(%d, %d, '%s')\n", cr->lz77.offset, cr->lz77.length, escape_char(cr->lz77.symbol));
}

