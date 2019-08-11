# Copyright (c) 2019 Daniel Abrecht
# SPDX-License-Identifier: LGPL-3.0-or-later

VERSION := $(shell cat version)
MAJOR   := $(word 1,$(subst ., ,$(VERSION)))
MINOR   := $(word 2,$(subst ., ,$(VERSION)))
PATCH   := $(word 3,$(subst ., ,$(VERSION)))

SOURCES += src/libconsolekeyboard.c

CC = gcc
AR = ar

PREFIX = /usr

OPTIONS += -ffunction-sections -fdata-sections -fPIC -shared -g -Og
CC_OPTS += -DLCK_BUILD -fvisibility=hidden -I include
CC_OPTS += -std=c99 -Wall -Wextra -pedantic -Werror
CC_OPTS += -D_DEFAULT_SOURCE
LD_OPTS += -Wl,-gc-sections

OBJS = $(patsubst src/%.c,build/%.o,$(SOURCES))

CC_OPTS += $(OPTIONS)
LD_OPTS += $(OPTIONS)

FILES_WITH_LIB_VERSION += debian/control
FILES_WITH_LIB_VERSION += $(wildcard debian/libconsolekeyboard*.*)


all: bin/libconsolekeyboard.a bin/libconsolekeyboard.so

%/.dir:
	mkdir -p "$(dir $@)"
	touch "$@"

release-major: release@$(shell expr $(MAJOR) + 1).0.0

release-minor: release@$(MAJOR).$(shell expr $(MINOR) + 1).0

release-patch: release@$(MAJOR).$(MINOR).$(shell expr $(PATCH) + 1)

release@%:
	set -x; \
	git clean -fdX debian/; \
	version="$(patsubst release@%,%,$@)"; \
	major="$(word 1,$(subst ., ,$(patsubst release@%,%,$@)))"; \
	minor="$(word 2,$(subst ., ,$(patsubst release@%,%,$@)))"; \
	patch="$(word 3,$(subst ., ,$(patsubst release@%,%,$@)))"; \
	dch -v "$$version"; \
	for file in $(FILES_WITH_LIB_VERSION); \
	do \
	  for prefix in libttymultiplex libttymultiplex.so.; \
	    do sed -i "s|\($$prefix\)\([0-9]\+\.[0-9]\+.[0-9]\+\)|\1$$major.$$minor.$$patch|g;s|\($$prefix\)\([0-9]\+\.[0-9]\+\)|\1$$major.$$minor|g;s|\($$prefix\)\([0-9]\+\)|\1$$major|g" "$$file"; \
	  done; \
	  for prefix in libttymultiplex libttymultiplex.so.; \
	  do \
	    if printf '%s\n' "$$file" | grep -q "$$prefix[0-9]\+"; \
	      then mv "$$file" "$$(printf '%s\n' "$$file" | sed "s|\($$prefix\)\([0-9]\+\.[0-9]\+.[0-9]\+\)|\1$$major.$$minor.$$patch|g;s|\($$prefix\)\([0-9]\+\.[0-9]\+\)|\1$$major.$$minor|g;s|\($$prefix\)\([0-9]\+\)|\1$$major|g")"; \
	    fi; \
	  done; \
	done; \
	echo "$$version" > version; \
	git add debian/; \
	git commit -m "New release v$$version"; \
	git tag "v$$version"

build/%.o: src/%.c | build/.dir
	$(CC) $(CC_OPTS) -c -o $@ $^

bin/libconsolekeyboard.a: $(OBJS) | bin/.dir
	$(AR) scr $@ $^

bin/libconsolekeyboard.so: bin/libconsolekeyboard.a
	$(CC) $(LD_OPTS) -Wl,--whole-archive $^ -Wl,--no-whole-archive $(LIBS) -o bin/libconsolekeyboard.so.$(MAJOR).$(MINOR).$(PATCH) -Wl,-soname,libconsolekeyboard.so.$(MAJOR)
	cd bin; \
	ln -sf "libconsolekeyboard.so.$(MAJOR).$(MINOR).$(PATCH)" "libconsolekeyboard.so.$(MAJOR).$(MINOR)"; \
	ln -sf "libconsolekeyboard.so.$(MAJOR).$(MINOR).$(PATCH)" "libconsolekeyboard.so.$(MAJOR)"; \
	ln -sf "libconsolekeyboard.so.$(MAJOR).$(MINOR).$(PATCH)" "libconsolekeyboard.so"

install:
	mkdir -p "$(DESTDIR)$(PREFIX)/lib"
	mkdir -p "$(DESTDIR)$(PREFIX)/include"
	cp bin/libconsolekeyboard.a "$(DESTDIR)$(PREFIX)/lib/libconsolekeyboard.a"
	cp bin/libconsolekeyboard.so "$(DESTDIR)$(PREFIX)/lib/libconsolekeyboard.so"
	cd "$(DESTDIR)$(PREFIX)/lib/"; \
	ln -sf "libconsolekeyboard.so.$(MAJOR).$(MINOR).$(PATCH)" "libconsolekeyboard.so.$(MAJOR).$(MINOR)"; \
	ln -sf "libconsolekeyboard.so.$(MAJOR).$(MINOR).$(PATCH)" "libconsolekeyboard.so.$(MAJOR)"; \
	ln -sf "libconsolekeyboard.so.$(MAJOR).$(MINOR).$(PATCH)" "libconsolekeyboard.so"
	cp include/libconsolekeyboard.h "$(DESTDIR)$(PREFIX)/include/"

uninstall:
	rm -f "$(DESTDIR)$(PREFIX)/lib/libconsolekeyboard.a"
	rm -f "$(DESTDIR)$(PREFIX)/lib/libconsolekeyboard.so"
	rm -f "$(DESTDIR)$(PREFIX)/include/libconsolekeyboard.h"

clean:
	rm -rf bin/ build/
