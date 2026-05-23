CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = `pkg-config --libs wayland-client`
INCLUDES = `pkg-config --cflags wayland-client`
WAYLAND_PROTOCOLS_DIR = /usr/share/wayland-protocols
XML_FILE = $(WAYLAND_PROTOCOLS_DIR)/staging/ext-idle-notify/ext-idle-notify-v1.xml

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

.PHONY: all clean
