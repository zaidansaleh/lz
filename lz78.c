/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2025 Saleh Zaidan */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lz.h"
#include "string.h"

typedef struct {
    uint16_t index;
    uint8_t symbol;
} LZ78_Tuple;

typedef struct LZ78_Node {
    LZ78_Tuple tuple;
    struct LZ78_Node *parent;
    struct LZ78_Node *child;
    struct LZ78_Node *sibling;
} LZ78_Node;

typedef struct {
    size_t capacity;
    size_t length;
    LZ78_Tuple data[];
} LZ78_TupleList;

LZ78_Node *lz78_node_new(uint16_t index, uint8_t symbol) {
    LZ78_Node *node = malloc(sizeof(LZ78_Node));
    if (!node) {
        return NULL;
    }
    node->tuple = (LZ78_Tuple){.index = index, .symbol = symbol};
    node->parent = NULL;
    node->child = NULL;
    node->sibling = NULL;
    return node;
}

void lz78_node_free(LZ78_Node *root) {
    LZ78_Node *node = root->child;
    while (node) {
        LZ78_Node *tmp = node;
        node = node->sibling;
        lz78_node_free(tmp);
    }
    free(root);
}

LZ78_Node *lz78_node_find_child(LZ78_Node *parent, uint8_t symbol) {
    if (!parent->child) {
        return NULL;
    }
    LZ78_Node *child = parent->child;
    while (child) {
        if (child->tuple.symbol == symbol) {
            return child;
        }
        child = child->sibling;
    }
    return NULL;
}

LZ78_Node *lz78_node_find_traverse(LZ78_Node *node, uint16_t index) {
    if (!node) {
        return NULL;
    }
    if (node->tuple.index == index) {
        return node;
    }
    LZ78_Node *child = lz78_node_find_traverse(node->child, index);
    if (child) {
        return child;
    }
    LZ78_Node *sibling = lz78_node_find_traverse(node->sibling, index);
    if (sibling) {
        return sibling;
    }
    return NULL;
}

void lz78_node_push(LZ78_Node *parent, LZ78_Node *child) {
    if (!parent->child) {
        parent->child = child;
        child->parent = parent;
    } else {
        LZ78_Node *node = parent->child;
        while (node->sibling) {
            node = node->sibling;
        }
        node->sibling = child;
        child->parent = node->parent;
    }
}

LZ78_TupleList *lz78_tuple_list_new(size_t capacity) {
    LZ78_TupleList *list = malloc(sizeof(LZ78_TupleList) + sizeof(LZ78_Tuple) * capacity);
    if (!list) {
        return NULL;
    }
    list->capacity = capacity;
    list->length = 0;
    memset(list->data, 0, sizeof(LZ78_Tuple) * capacity);
    return list;
}

int lz78_tuple_list_push(LZ78_TupleList *list, const LZ78_Tuple *tuple) {
    if (list->length >= list->capacity) {
        return -1;
    }
    list->data[list->length++] = *tuple;
    return 0;
}

int lz78_serialize(const void *compressed, FILE *stream) {
    const LZ78_TupleList *list = compressed;
    for (size_t i = 0; i < list->length; ++i) {
        const LZ78_Tuple *tuple = &list->data[i];
        uint8_t buf[3];
        uint16_be_write(buf, tuple->index);
        uint8_be_write(buf + 2, tuple->symbol);
        size_t written = fwrite(buf, 1, sizeof(buf), stream);
        if (written != sizeof(buf)) {
            return -1;
        }
    }
    return 0;
}

void *lz78_deserialize(FILE *stream) {
    bool error = false;
    LZ78_TupleList *list = NULL;

    fseek(stream, 0, SEEK_END);
    long file_size = ftell(stream);
    rewind(stream);

    list = lz78_tuple_list_new(file_size);
    if (!list) {
        error = true;
        goto cleanup;
    }

    while (true) {
        uint8_t buf[3];
        size_t read = fread(buf, 1, sizeof(buf), stream);
        if (read != sizeof(buf)) {
            if (feof(stream)) {
                break;
            }
            error = true;
            goto cleanup;
        }
        LZ78_Tuple tuple = {
            .index = uint16_be_read(buf),
            .symbol = uint8_be_read(buf + 2),
        };
        if (lz78_tuple_list_push(list, &tuple) < 0) {
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

void *lz78_compress(const String *input) {
    bool error = false;
    LZ78_Node *root = NULL;
    LZ78_TupleList *list = NULL;

    list = lz78_tuple_list_new(input->length);
    if (!list) {
        error = true;
        goto cleanup;
    }

    root = lz78_node_new(0, '\0');
    if (!root) {
        error = true;
        goto cleanup;
    }

    size_t next_index = 1;
    LZ78_Node *last_match_node = root;
    for (size_t i = 0; i <= input->length; ++i) {
        const char symbol = i == input->length ? '\0' : input->data[i];
        LZ78_Node *node = lz78_node_find_child(last_match_node, symbol);
        if (node) {
            last_match_node = node;
        } else {
            size_t index = next_index++;
            LZ78_Node *child = lz78_node_new(index, symbol);
            lz78_node_push(last_match_node, child);
            LZ78_Tuple tuple = {.index = last_match_node->tuple.index, .symbol = symbol};
            if (lz78_tuple_list_push(list, &tuple) < 0) {
                error = true;
                goto cleanup;
            }
            last_match_node = root;
        }
    }

cleanup:
    lz78_node_free(root);
    if (error) {
        lz78_free(list);
        return NULL;
    }
    return list;
}

int lz78_tuple_list_resolve(String *out, LZ78_Node *node) {
    while (node && node->tuple.symbol != '\0') {
        if (string_push(out, node->tuple.symbol) < 0) {
            return -1;
        }
        node = node->parent;
    }
    return 0;
}

String *lz78_decompress(const void *compressed) {
    const LZ78_TupleList *list = compressed;
    bool error = false;
    String *buf = NULL;
    LZ78_Node *root = NULL;

    buf = string_new();
    if (!buf) {
        error = true;
        goto cleanup;
    }

    root = lz78_node_new(0, '\0');
    if (!root) {
        error = true;
        goto cleanup;
    }

    size_t next_index = 1;
    for (size_t i = 0; i < list->length; ++i) {
        const LZ78_Tuple *tuple = &list->data[i];
        LZ78_Node *node = root;
        if (tuple->index != 0) {
            node = lz78_node_find_traverse(root, tuple->index);
            if (!node) {
                error = true;
                goto cleanup;
            }
            String *prefix = string_new();
            if (!prefix) {
                error = true;
                goto cleanup;
            }
            if (lz78_tuple_list_resolve(prefix, node) < 0) {
                error = true;
                goto cleanup;
            }
            for (size_t j = prefix->length; j-- > 0;) {
                if (string_push(buf, prefix->data[j]) < 0) {
                    error = true;
                    goto cleanup;
                }
            }
            string_free(prefix);
        }
        if (tuple->symbol != '\0') {
            if (string_push(buf, tuple->symbol) < 0) {
                error = true;
                goto cleanup;
            }
        }
        LZ78_Node *child = lz78_node_new(next_index++, tuple->symbol);
        lz78_node_push(node, child);
    }

cleanup:
    lz78_node_free(root);
    if (error) {
        string_free(buf);
        return NULL;
    }
    return buf;
}

void lz78_print(const void *compressed, FILE *stream) {
    const LZ78_TupleList *list = compressed;
    for (size_t i = 0; i < list->length; ++i) {
        const LZ78_Tuple *tuple = &list->data[i];
        fprintf(stream, "(%d, '%s')\n", tuple->index, escape_char(tuple->symbol));
    }
}

void lz78_free(void *compressed) {
    free(compressed);
}

