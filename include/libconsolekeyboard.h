// Copyright (c) 2018 Daniel Abrecht
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef LIBCONSOLEKEYBOARD_H
#define LIBCONSOLEKEYBOARD_H

#include <stddef.h>
#include <stdint.h>

#ifndef LCK_EXPORT
#ifdef LCK_BUILD
#define LCK_EXPORT __attribute__((visibility ("default")))
#else
#define LCK_EXPORT
#endif
#endif

#define TYM_CONCAT_SUB(A,B) A ## B
#define TYM_CONCAT(A,B) TYM_CONCAT_SUB(A,B)

#ifdef __cplusplus
extern "C" {
#endif

enum lck_cmd {
  LCK_SEND_KEY    = 0x01,
  LCK_SEND_STRING = 0x02,
  LCK_SET_HEIGHT  = 0x03,
};

struct lck_super_size {
  uint64_t character;
  // This may be extended with additional parameters related to other units
  // They will all be added together in the console multiplexer
};

LCK_EXPORT int lck_send_cmd(enum lck_cmd cmd, size_t size, uint8_t data[size]);
LCK_EXPORT int lck_send_key(const char* key);
LCK_EXPORT int lck_send_string(const char* key);
LCK_EXPORT int lck_set_height(struct lck_super_size size);

#ifdef __cplusplus
}
#endif

#endif
