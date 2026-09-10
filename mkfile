CC = cc
CFLAGS = -Wall -Wextra -O2 -std=c11
PREFIX = /usr/local

mfetch: src/gfetch.c
	$(CC) $(CFLAGS) -o mfetch src/gfetch.c

install: mfetch
	cp mfetch $(PREFIX)/bin/mfetch

uninstall:
	rm -f $(PREFIX)/bin/mfetch

clean:
	rm -f mfetch
