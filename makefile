# Copyright (c) 2018 Daniel Abrecht
# SPDX-License-Identifier: AGPL-3.0-or-later

CC = gcc
AR = ar

PREFIX = /usr

OPTIONS += -ffunction-sections -fdata-sections -g -Og
CC_OPTS += -fvisibility=hidden -I include
CC_OPTS += -std=c99 -Wall -Wextra -pedantic -Werror
CC_OPTS += -D_DEFAULT_SOURCE
LD_OPTS += -Wl,-gc-sections

LD_OPTS += -lttymultiplex

CC_OPTS += $(OPTIONS)
LD_OPTS += $(OPTIONS)

all: bin/console-keyboard-multiplexer

%/.dir:
	mkdir -p "$(dir $@)"
	touch "$@"

build/%.o: src/%.c | build/.dir
	$(CC) $(CC_OPTS) -c -o $@ $^

bin/console-keyboard-multiplexer: build/console-keyboard-multiplexer.o
	mkdir -p bin
	$(CC) $(LD_OPTS) $^ -o $@

install:
	mkdir -p "$(DESTDIR)$(PREFIX)/bin/"
	cp bin/console-keyboard-multiplexer "$(DESTDIR)$(PREFIX)/bin/console-keyboard-multiplexer"

uninstall:
	rm -f "$(DESTDIR)$(PREFIX)/bin/console-keyboard-multiplexer"

clean:
	rm -rf bin/ build/
