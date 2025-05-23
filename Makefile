lz: lz.c
	gcc -std=c11 -Wall -Wextra -g -fsanitize=address -o lz lz.c

.PHONY: clean
clean:
	rm lz

