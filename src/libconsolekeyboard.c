// Copyright (c) 2019 Daniel Abrecht
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <libconsolekeyboard.h>

int lck_send_cmd(enum lck_cmd cmd, size_t size, uint8_t data[size]){
  if(size > 254 || (unsigned)cmd > 255)
    return -1;
  while(true){
    ssize_t ret = write(3, (uint8_t[]){size+1}, 1);
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
  size_t n = 0;
  while(true){
    ssize_t ret = write(3, data+n, size-n);
    if(ret == 0)
      continue;
    if(ret == -1 && (errno == EAGAIN || errno == EINTR))
      continue;
    if(ret < 0)
      return -1;
    if((size_t)ret > size-n)
      return -1;
    n += ret;
    if(n < size)
      continue;
    break;
  }
  return 0;
}

int lck_send_key(const char* key){
  return lck_send_cmd(LCK_SEND_KEY, strlen(key), (uint8_t*)key);
}

int lck_send_string(const char* key){
  return lck_send_cmd(LCK_SEND_STRING, strlen(key), (uint8_t*)key);
}
