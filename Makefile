lz: lz.c
	gcc -std=c11 -Wall -Wextra -g -o lz lz.c

.PHONY: clean
clean:
	rm lz

