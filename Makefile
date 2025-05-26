lz: lz.c string.c lz77.c
	gcc -std=c11 -Wall -Wextra -g -fsanitize=address -o lz lz.c string.c lz77.c

.PHONY: clean
clean:
	rm lz

