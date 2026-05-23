CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = `pkg-config --libs wayland-client`
INCLUDES = `pkg-config --cflags wayland-client`
WAYLAND_PROTOCOLS_DIR = $(shell pkg-config --variable=pkgdatadir wayland-protocols)
XML_FILE = $(WAYLAND_PROTOCOLS_DIR)/staging/ext-idle-notify/ext-idle-notify-v1.xml
PREFIX ?= /usr/local

all: qtile-lock

qtile-lock: qtile_lock.o ext-idle-notify-v1-protocol.o
	$(CC) $(CFLAGS) -o qtile-lock qtile_lock.o ext-idle-notify-v1-protocol.o $(LDFLAGS)

qtile_lock.o: qtile_lock.c ext-idle-notify-v1-client-protocol.h
	$(CC) $(CFLAGS) $(INCLUDES) -c qtile_lock.c

ext-idle-notify-v1-protocol.o: ext-idle-notify-v1-protocol.c ext-idle-notify-v1-client-protocol.h
	$(CC) $(CFLAGS) $(INCLUDES) -c ext-idle-notify-v1-protocol.c

ext-idle-notify-v1-client-protocol.h: $(XML_FILE)
	wayland-scanner client-header $(XML_FILE) ext-idle-notify-v1-client-protocol.h

ext-idle-notify-v1-protocol.c: $(XML_FILE)
	wayland-scanner private-code $(XML_FILE) ext-idle-notify-v1-protocol.c

clean:
	rm -f qtile-lock *.o ext-idle-notify-v1-client-protocol.h ext-idle-notify-v1-protocol.c

install: qtile-lock
	install -Dm755 qtile-lock $(DESTDIR)$(PREFIX)/bin/qtile-lock

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/qtile-lock

.PHONY: all clean install uninstall
