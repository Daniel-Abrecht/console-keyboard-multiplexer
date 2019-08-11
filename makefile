# Copyright (c) 2018 Daniel Abrecht
# SPDX-License-Identifier: AGPL-3.0-or-later

CC = gcc
AR = ar

PREFIX = /usr

OPTIONS += -ffunction-sections -fdata-sections -g -Og
CC_OPTS += -fvisibility=hidden -I include
CC_OPTS += -std=c99 -Wall -Wextra -pedantic -Werror
CC_OPTS += -D_DEFAULT_SOURCE
CC_OPTS += -DTYM_LOG_PROJECT='"console-keyboard-multiplexer"'
LD_OPTS += -Wl,-gc-sections

LIBS += -lttymultiplex

CC_OPTS += $(OPTIONS)
LD_OPTS += $(OPTIONS)

OBJECTS += build/console-keyboard-multiplexer.o
OBJECTS += build/man/console-keyboard-multiplexer.1.res.o

all: bin/console-keyboard-multiplexer

%/.dir:
	mkdir -p "$(dir $@)"
	touch "$@"

build/%.o: src/%.c | build/.dir
	$(CC) $(CC_OPTS) -c -o $@ $^

build/%.res.o: % | build/%/.dir
	file="$^"; \
	id="res_$$(printf '%s' "$$file"|sed 's/[^a-zA-Z0-9]/_/g'|sed 's/.*/\L\0/')"; \
	( \
	  echo '#include <stddef.h>'; \
	  printf "extern const char %s[];" "$$id"; \
	  printf "extern const size_t %s_size;" "$$id"; \
	  printf "const char %s[] = {" "$$id"; \
	  cat "$$file" | sed 's/\\/\\\\/g' | sed 's/"/\\"/g' | sed 's/.*/  "\0\\n"/'; \
	  printf "};\nconst size_t %s_size = sizeof(%s)-1;\n" "$$id" "$$id"; \
	) | $(CC) -x c - -c -o $@

bin/console-keyboard-multiplexer: $(OBJECTS)
	mkdir -p bin
	$(CC) $(LD_OPTS) $^ $(LIBS) -o $@

install: install-bin install-config install-initramfs-tools-config
	@true

install-bin:
	mkdir -p "$(DESTDIR)$(PREFIX)/bin/"
	cp bin/console-keyboard-multiplexer "$(DESTDIR)$(PREFIX)/bin/console-keyboard-multiplexer"

install-config:
	mkdir -p "$(DESTDIR)$(PREFIX)/lib/systemd/system/getty@.service.d/"
	cp config/console-keyboard-multiplexer-systemd-override.conf "$(DESTDIR)$(PREFIX)/lib/systemd/system/getty@.service.d/console-keyboard-multiplexer.conf"

install-initramfs-tools-config:
	for file in \
	  hooks/consolation \
	  scripts/init-bottom/consolation \
	  scripts/init-premount/consolation \
	  hooks/console-keyboard-multiplexer \
	  scripts/init-bottom/console-keyboard-multiplexer \
	  scripts/init-premount/console-keyboard-multiplexer; \
	do \
	  mkdir -p "$$(dirname "$(DESTDIR)/usr/share/initramfs-tools/$$file")"; \
	  cp -r "config/initramfs-tools/$$file" "$(DESTDIR)/usr/share/initramfs-tools/$$file"; \
	done

uninstall:
	rm -f "$(DESTDIR)$(PREFIX)/bin/console-keyboard-multiplexer"
	rm -f "$(DESTDIR)$(PREFIX)/lib/systemd/system/getty@.service.d/console-keyboard-multiplexer.conf"
	for file in \
	  hooks/consolation \
	  scripts/init-bottom/consolation \
	  scripts/init-premount/consolation \
	  hooks/console-keyboard-multiplexer \
	  scripts/init-bottom/console-keyboard-multiplexer \
	  scripts/init-premount/console-keyboard-multiplexer; \
	do \
	  rm "$(DESTDIR)/usr/share/initramfs-tools/$$file"; \
	done

clean:
	rm -rf bin/ build/
