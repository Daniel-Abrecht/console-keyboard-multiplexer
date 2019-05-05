// Copyright (c) 2018 Daniel Abrecht
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef LIBCONSOLEKEYBOARD_H
#define LIBCONSOLEKEYBOARD_H

#include <stddef.h>
#include <stdint.h>

enum lck_cmd {
  LCK_SEND_KEY    = 0x01,
  LCK_SEND_STRING = 0x02,
};

int lck_send_cmd(enum lck_cmd cmd, size_t size, uint8_t data[size]);
int lck_send_key(const char* key);
int lck_send_string(const char* key);

#endif
