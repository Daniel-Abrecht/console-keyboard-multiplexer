// Copyright (c) 2019 Daniel Abrecht
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <libconsolekeyboard.h>

struct buffer {
  size_t size;
  uint8_t* data;
};

int lck_send_cmd_multi(enum lck_cmd cmd, size_t n, struct buffer b[n]){
  if(cmd > 255 || cmd < 0)
    return -1;
  size_t fullsize = 0;
  for(size_t i=0; i<n; i++){
    if(b[i].size > 254 || fullsize > 254-b[i].size)
      return -1;
    fullsize += b[i].size;
  }
  while(true){
    ssize_t ret = write(3, (uint8_t[]){fullsize+1}, 1);
    if(ret == 0)
      continue;
    if(ret == -1 && (errno == EAGAIN || errno == EINTR))
      continue;
    if(ret > 1 || ret < 0)
      return -1;
    break;
  }
  while(true){
    ssize_t ret = write(3, (uint8_t[]){cmd}, 1);
    if(ret == 0)
      continue;
    if(ret == -1 && (errno == EAGAIN || errno == EINTR))
      continue;
    if(ret > 1 || ret < 0)
      return -1;
    break;
  }
  for(size_t i=0; i<n; i++){
    size_t m = 0;
    size_t size = b[i].size;
    uint8_t* data = b[i].data;
    while(true){
      ssize_t ret = write(3, data+m, size-m);
      if(ret == 0)
        continue;
      if(ret == -1 && (errno == EAGAIN || errno == EINTR))
        continue;
      if(ret < 0)
        return -1;
      if((size_t)ret > size-m)
        return -1;
      m += ret;
      if(m < size)
        continue;
      break;
    }
  }
  return 0;
}

int lck_send_cmd(enum lck_cmd cmd, size_t size, uint8_t data[size]){
  return lck_send_cmd_multi(cmd, 1, (struct buffer[]){
    { size, data }
  });
}

int lck_send_key(const char* key){
  return lck_send_cmd(LCK_SEND_KEY, strlen(key), (uint8_t*)key);
}

int lck_send_string(const char* string, enum lck_key_modifier_mask modifier){
  return lck_send_cmd_multi(LCK_SEND_STRING, 2, (struct buffer[]){
    { 1, (uint8_t[]){modifier} },
    { strlen(string), (uint8_t*)string }
  });
}

static void uint64_to_bytes_be(uint8_t out[8], uint64_t x){
  out[0] = x >> 56;
  out[1] = x >> 48;
  out[2] = x >> 40;
  out[3] = x >> 32;
  out[4] = x >> 24;
  out[5] = x >> 16;
  out[6] = x >> 8;
  out[7] = x;
}

int lck_set_height(struct lck_super_size size){
  uint8_t buffer[16];
  uint64_to_bytes_be(buffer, size.character);
  return lck_send_cmd(LCK_SET_HEIGHT, 8, buffer);
}
