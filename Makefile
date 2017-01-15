prefix=/usr/local
CC=gcc
GLIB_FLAGS=-std=c11 -Werror -O2 $(shell pkg-config --cflags --libs glib-2.0)
orez: main.c
	$(CC) $(GLIB_FLAGS) $< -o $@

.PHONY: install uninstall clean
install:
	install -c orez orez-ctx orez-md $(prefix)/bin
uninstall:
	rm $(prefix)/bin/orez
	rm $(prefix)/bin/orez-ctx
	rm $(prefix)/bin/orez-md
clean:
	rm orez
