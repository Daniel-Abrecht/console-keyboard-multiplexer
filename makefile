# Copyright (c) 2018 Daniel Abrecht
# SPDX-License-Identifier: AGPL-3.0-or-later

CC = gcc
AR = ar

PREFIX = /usr

OPTIONS  = -ffunction-sections -fdata-sections -g -Og
CC_OPTS  = -fvisibility=hidden -I include -I repo/libttymultiplex/include
CC_OPTS += -std=c99 -Wall -Wextra -pedantic -Werror -D_DEFAULT_SOURCE
LD_OPTS  = -Wl,-gc-sections -lutil

LD_OPTS += -Lrepo/libttymultiplex/bin/ -lttymultiplex

CC_OPTS += $(OPTIONS)
LD_OPTS += $(OPTIONS)

all: bin/console-keyboard-multiplexer

%/.dir:
	mkdir -p "$(dir $@)"
	touch "$@"

build/%.o: src/%.c | build/.dir
	$(CC) $(CC_OPTS) -c -o $@ $^

repo/libttymultiplex/.repo: repo/.dir
	git clone git@gitlab.com:DanielAbrecht/libttymultiplex.git repo/libttymultiplex/
	touch repo/libttymultiplex/.repo

repo/libttymultiplex/bin/libttymultiplex.so: repo/libttymultiplex/.repo
	make -C repo/libttymultiplex/

bin/console-keyboard-multiplexer: build/console-keyboard-multiplexer.o bin/.dir repo/libttymultiplex/bin/libttymultiplex.so
	$(CC) $(LD_OPTS) -Wl,--whole-archive $< -Wl,--no-whole-archive -o $@

install:
	cp bin/console-keyboard-multiplexer $(PREFIX)/bin/console-keyboard-multiplexer

uninstall:
	rm -f $(PREFIX)/bin/console-keyboard-multiplexer

clean:
	rm -rf bin/ build/
