CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99

all: open5G_cli

open5G_cli: main.c
	$(CC) $(CFLAGS) $< -o $@ -lncurses

clean:
	rm -f open5G_cli
