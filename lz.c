#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint16_t offset;
    uint8_t length;
    uint8_t symbol;
} Tuple;

int main(void) {
    const char *input = "abracadabrad";
    size_t input_length = strlen(input);
    size_t position = 0;
    while (position < input_length) {
        const char *a = &input[position];
        uint16_t max_offset = 0;
        uint8_t max_length = 0;
        for (int i = position - 1; i >= 0; --i) {
            for (size_t j = i; j < position - 1; ++j) {
                const char *b = &input[j];
                size_t length = 0;
                while (*(b + length) == *(a + length)) {
                    length += 1;
                    if (length > max_length) { 
                        max_length = length;
                        max_offset = position - i;
                    }
                }
            }
        }
        Tuple tuple = {
            .offset = max_offset,
            .length = max_length,
            .symbol = input[position + max_length],
        };
        printf("(%d, %d, '%c')\n", tuple.offset, tuple.length, tuple.symbol);
        position += max_length + 1;
    }
    return 0;
}

