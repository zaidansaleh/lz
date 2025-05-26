lz: lz.c string.c
	gcc -std=c11 -Wall -Wextra -g -fsanitize=address -o lz lz.c string.c

.PHONY: clean
clean:
	rm lz

