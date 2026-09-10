CC = cc
CFLAGS = -Wall -Wextra -O2 -std=c11
PREFIX = /usr/local

mfetch: src/mfetch.c
	$(CC) $(CFLAGS) -o mfetch src/mfetch.c

install: mfetch
	cp mfetch $(PREFIX)/bin/mfetch

uninstall:
	rm -f $(PREFIX)/bin/mfetch

clean:
	rm -f mfetch
