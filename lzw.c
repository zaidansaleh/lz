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

typedef struct LZW_Entry {
    String *prefix;
    uint16_t code;
    struct LZW_Entry *next;
} LZW_Entry;

typedef struct {
    size_t capacity;
    LZW_Entry *data[];
} LZW_HashTable;

typedef struct {
    size_t capacity;
    size_t length;
    uint16_t data[];
} LZW_CodeList;

typedef struct {
    size_t capacity;
    String *data[];
} LZW_PrefixTable;

LZW_Entry *lzw_entry_new(String *prefix, uint16_t code) {
    LZW_Entry *entry = malloc(sizeof(LZW_Entry));
    if (!entry) {
        return NULL;
    }
    entry->prefix = prefix;
    entry->code = code;
    entry->next = NULL;
    return entry;
}

void lzw_entry_free(LZW_Entry *entry) {
    if (entry->next) {
        lzw_entry_free(entry->next);
    }
    string_free(entry->prefix);
    free(entry);
}

void lzw_entry_print(const LZW_Entry *entry, FILE *stream) {
    fprintf(stream, "(prefix: ");
    for (size_t i = 0; i < entry->prefix->length; ++i) {
        char ch = entry->prefix->data[i];
        if (isprint(ch)) {
            fputc(ch, stream);
        } else {
            fprintf(stream, "<%d>", ch);
        }
    }
    fprintf(stream, ", code: %hu)", entry->code);
}

LZW_HashTable *lzw_hash_table_new(size_t capacity) {
    LZW_HashTable *table = malloc(sizeof(LZW_HashTable) + sizeof(LZW_Entry) * capacity);
    if (!table) {
        return NULL;
    }
    table->capacity = capacity;
    memset(table->data, 0, sizeof(LZW_Entry) * capacity);
    return table;
}

void lzw_hash_table_free(LZW_HashTable *table) {
    for (size_t i = 0; i < table->capacity; ++i) {
        LZW_Entry *entry = table->data[i];
        if (entry) {
            lzw_entry_free(entry);
        }
    }
    free(table);
}

size_t lzw_hash_table_hash_prefix(const LZW_HashTable *table, const String *prefix) {
    // TODO: Use a better hash function.
    size_t n = 0;
    for (size_t i = 0; i < prefix->length; ++i) {
        n = n << 1 | prefix->data[i];
    }
    return n % table->capacity;
}

LZW_Entry *lzw_hash_table_get(const LZW_HashTable *table, const String *prefix) {
    size_t index = lzw_hash_table_hash_prefix(table, prefix);
    LZW_Entry *entry = table->data[index];
    if (!entry) {
        return NULL;
    }
    while (entry) {
        if (entry->prefix->length == prefix->length &&
            strncmp(entry->prefix->data, prefix->data, prefix->length) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

void lzw_hash_table_insert(LZW_HashTable *table, String *prefix, uint16_t code) {
    size_t index = lzw_hash_table_hash_prefix(table, prefix);
    LZW_Entry *new_entry = lzw_entry_new(prefix, code);
    LZW_Entry *entry = table->data[index];
    if (!entry) {
        table->data[index] = new_entry;
        return;
    }
    while (entry->next) {
        entry = entry->next;
    }
    entry->next = new_entry;
}

void lzw_hash_table_pprint(const LZW_HashTable *table, FILE *stream) {
    for (size_t i = 0; i < table->capacity; ++i) {
        LZW_Entry *entry = table->data[i];
        while (entry) {
            lzw_entry_print(entry, stream);
            if (entry->next) {
                fprintf(stream, " -> ");
            }
            entry = entry->next;
        }
        fputc('\n', stream);
    }
}

LZW_CodeList *lzw_code_list_new(size_t capacity) {
    LZW_CodeList *list = malloc(sizeof(LZW_CodeList) + sizeof(uint16_t) * capacity);
    if (!list) {
        return NULL;
    }
    list->capacity = capacity;
    list->length = 0;
    memset(list->data, 0, sizeof(uint16_t) * capacity);
    return list;
}

void lzw_code_list_free(LZW_CodeList *list) {
    free(list);
}

int lzw_code_list_push(LZW_CodeList *list, uint16_t code) {
    if (list->length >= list->capacity) {
        return -1;
    }
    list->data[list->length++] = code;
    return 0;
}

LZW_PrefixTable *lzw_prefix_table_new(size_t capacity) {
    LZW_PrefixTable *table = malloc(sizeof(LZW_PrefixTable) + sizeof(String *) * capacity);
    if (!table) {
        return NULL;
    }
    table->capacity = capacity;
    memset(table->data, 0, sizeof(String *) * capacity);
    return table;
}

void lzw_prefix_table_free(LZW_PrefixTable *table) {
    for (size_t i = 0; i < table->capacity; ++i) {
        String *s = table->data[i];
        if (s) {
            string_free(s);
        }
    }
    free(table);
}

String *lzw_prefix_table_get(LZW_PrefixTable *table, uint16_t code) {
    return table->data[code];
}

int lzw_prefix_table_insert(LZW_PrefixTable *table, uint16_t code, String *prefix) {
    if (code >= table->capacity) {
        return -1;
    }
    table->data[code] = prefix;
    return 0;
}

int lzw_serialize(const void *compressed, FILE *stream) {
    const LZW_CodeList *list = compressed;
    for (size_t i = 0; i < list->length; ++i) {
        uint8_t buf[2];
        uint16_be_write(buf, list->data[i]);
        size_t written = fwrite(buf, 1, sizeof(buf), stream);
        if (written != sizeof(buf)) {
            return -1;
        }
    }
    return 0;
}

void *lzw_deserialize(FILE *stream) {
    bool error = false;
    LZW_CodeList *list = NULL;

    fseek(stream, 0, SEEK_END);
    long file_size = ftell(stream);
    rewind(stream);

    list = lzw_code_list_new(file_size);
    if (!list) {
        error = true;
        goto cleanup;
    }

    while (true) {
        uint8_t buf[2];
        size_t read = fread(buf, 1, sizeof(buf), stream);
        if (read != sizeof(buf)) {
            if (feof(stream)) {
                break;
            }
            error = true;
            goto cleanup;
        }
        uint16_t code = uint16_be_read(buf);
        if (lzw_code_list_push(list, code) < 0) {
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

void *lzw_compress(const String *input) {
    bool error = false;
    LZW_CodeList *list = NULL;
    LZW_HashTable *dict = NULL;
    String *seq = NULL;

    list = lzw_code_list_new(input->length);
    if (!list) {
        error = true;
        goto cleanup;
    }

    dict = lzw_hash_table_new(UINT8_MAX + 1);
    if (!dict) {
        error = true;
        goto cleanup;
    }
    uint16_t next_code = 0;
    for (int ch = 0; ch <= UINT8_MAX; ++ch) {
        String *s = string_new();
        if (!s) {
            error = true;
            goto cleanup;
        }
        if (string_push(s, ch) < 0) {
            error = true;
            goto cleanup;
        }
        lzw_hash_table_insert(dict, s, next_code++);
    }

    seq = string_new();
    if (!seq) {
        error = true;
        goto cleanup;
    }

    for (size_t i = 0; i < input->length; ++i) {
        const char symbol = input->data[i];
        String *candidate = string_copy(seq);
        if (!candidate) {
            error = true;
            goto cleanup;
        }
        if (string_push(candidate, symbol) < 0) {
            error = true;
            goto cleanup;
        }

        LZW_Entry *candidate_entry = lzw_hash_table_get(dict, candidate);
        if (candidate_entry) {
            string_free(seq);
            seq = candidate;
        } else {
            LZW_Entry *seq_entry = lzw_hash_table_get(dict, seq);
            if (!seq_entry) {
                error = true;
                goto cleanup;
            }
            if (lzw_code_list_push(list, seq_entry->code) < 0) {
                error = true;
                goto cleanup;
            }
            lzw_hash_table_insert(dict, candidate, next_code++);
            string_clear(seq);
            if (string_push(seq, symbol) < 0) {
                error = true;
                goto cleanup;
            }
        }
    }

    if (seq->length > 0) {
        LZW_Entry *seq_entry = lzw_hash_table_get(dict, seq);
        if (!seq_entry) {
            error = true;
            goto cleanup;
        }
        if (lzw_code_list_push(list, seq_entry->code) < 0) {
            error = true;
            goto cleanup;
        }
    }

cleanup:
    string_free(seq);
    lzw_hash_table_free(dict);
    if (error) {
        lzw_code_list_free(list);
        return NULL;
    }
    return list;
}

String *lzw_decompress(const void *compressed) {
    const LZW_CodeList *list = compressed;
    bool error = false;
    String *result = NULL;
    String *prev = NULL;
    LZW_PrefixTable *dict = NULL;

    result = string_new();
    if (!result) {
        error = true;
        goto cleanup;
    }

    dict = lzw_prefix_table_new(UINT8_MAX + list->length);
    if (!dict) {
        error = true;
        goto cleanup;
    }
    uint16_t next_code = 0;
    for (int ch = 0; ch <= UINT8_MAX; ++ch) {
        String *s = string_new();
        if (!s) {
            error = true;
            goto cleanup;
        }
        if (string_push(s, ch) < 0) {
            error = true;
            goto cleanup;
        }
        if (lzw_prefix_table_insert(dict, next_code++, s) < 0) {
            error = true;
            goto cleanup;
        }
    }

    for (size_t i = 0; i < list->length; ++i) {
        uint16_t code = list->data[i];
        String *seq = lzw_prefix_table_get(dict, code);
        bool is_seq_allocated = false;
        if (!seq && prev) {
            seq = string_copy(prev);
            if (!seq) {
                error = true;
                goto cleanup;
            }
            if (string_push(seq, prev->data[0]) < 0) {
                error = true;
                goto cleanup;
            }
            is_seq_allocated = true;
        }
        for (size_t j = 0; j < seq->length; ++j) {
            if (string_push(result, seq->data[j]) < 0) {
                error = true;
                goto cleanup;
            }
        }
        if (prev) {
            String *candidate = string_copy(prev);
            if (!candidate) {
                error = true;
                goto cleanup;
            }
            if (string_push(candidate, seq->data[0]) < 0) {
                error = true;
                goto cleanup;
            }
            if (lzw_prefix_table_insert(dict, next_code++, candidate) < 0) {
                error = true;
                goto cleanup;
            }
            string_free(prev);
        }
        if (is_seq_allocated) {
            prev = seq;
        } else {
            prev = string_copy(seq);
        }
    }

cleanup:
    lzw_prefix_table_free(dict);
    string_free(prev);
    if (error) {
        string_free(result);
        return NULL;
    }
    return result;
}

void lzw_print(const void *compressed, FILE *stream) {
    const LZW_CodeList *list = compressed;
    for (size_t i = 0; i < list->length; ++i) {
        uint16_t code = list->data[i];
        fprintf(stream, "%hu\n", code);
    }
}

void lzw_free(void *compressed) {
    lzw_code_list_free(compressed);
}

