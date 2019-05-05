# Copyright (c) 2018 Daniel Abrecht
# SPDX-License-Identifier: LGPL-3.0-or-later

SOURCES += src/libconsolekeyboard.c

CC = gcc
AR = ar

PREFIX = /usr

OPTIONS += -ffunction-sections -fdata-sections -fPIC -shared -g -Og
CC_OPTS += -fvisibility=hidden -I include
CC_OPTS += -std=c99 -Wall -Wextra -pedantic -Werror
CC_OPTS += -D_DEFAULT_SOURCE
LD_OPTS += -Wl,-gc-sections

OBJS = $(patsubst src/%.c,build/%.o,$(SOURCES))

CC_OPTS += $(OPTIONS)
LD_OPTS += $(OPTIONS)

all: bin/libconsolekeyboard.a bin/libconsolekeyboard.so

%/.dir:
	mkdir -p "$(dir $@)"
	touch "$@"

build/%.o: src/%.c | build/.dir
	$(CC) $(CC_OPTS) -c -o $@ $^

bin/libconsolekeyboard.a: $(OBJS) | bin/.dir
	$(AR) scr $@ $^

bin/libconsolekeyboard.so: bin/libconsolekeyboard.a
	$(CC) $(LD_OPTS) -Wl,--whole-archive $^ -Wl,--no-whole-archive $(LIBS) -o $@

install:
	mkdir -p "$(DESTDIR)$(PREFIX)/lib"
	mkdir -p "$(DESTDIR)$(PREFIX)/include"
	cp bin/libconsolekeyboard.a "$(DESTDIR)$(PREFIX)/lib/libconsolekeyboard.a"
	cp bin/libconsolekeyboard.so "$(DESTDIR)$(PREFIX)/lib/libconsolekeyboard.so"
	cp include/libconsolekeyboard.h "$(DESTDIR)$(PREFIX)/include/"

uninstall:
	rm -f "$(DESTDIR)$(PREFIX)/lib/libconsolekeyboard.a"
	rm -f "$(DESTDIR)$(PREFIX)/lib/libconsolekeyboard.so"
	rm -f "$(DESTDIR)$(PREFIX)/include/libconsolekeyboard.h"

clean:
	rm -rf bin/ build/
