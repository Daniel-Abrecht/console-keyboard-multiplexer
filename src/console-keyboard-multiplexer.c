// Copyright (c) 2018 Daniel Abrecht
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <libttymultiplex.h>

int keyboard_size;

int top_pane = -1;
struct tym_superposition top_pane_coordinates = {
  .position = { [TYM_P_RATIO] = { [1] = { .axis = {
    { .value = { .real = 1 } },
    { .value = { .real = 1 } }
  }
}}}};

int bottom_pane = -1;
struct tym_superposition bottom_pane_coordinates = {
  .position = { [TYM_P_RATIO] = {
    { .axis = {
      { .value = { .real = 0 } },
      { .value = { .real = 1 } }
    }},
    { .axis = {
      { .value = { .real = 1 } },
      { .value = { .real = 1 } }
    }}
  }
}};

void set_keyboard_size(int size){
  keyboard_size = size;
  top_pane_coordinates.position[TYM_P_CHARFIELD][1].axis[1].value.integer = -size;
  bottom_pane_coordinates.position[TYM_P_CHARFIELD][0].axis[1].value.integer = -size;
  if(top_pane != -1){
    if( tym_pane_resize(top_pane, &top_pane_coordinates) == -1){
//      perror("tym_resize_pane failed");
    }
  }
  if(bottom_pane != -1){
    if( tym_pane_resize(bottom_pane, &bottom_pane_coordinates) == -1){
//      perror("tym_resize_pane failed");
    }
  }
}

int forkcall(void* ptr, int(*init)(void*ptr), char* args[]){
  int ret = fork();
  if(ret == -1)
    return -1;
  if(ret)
    return ret;
  ret = (*init)(ptr);
  if(ret == -1)
    exit(errno);
  // TODO: use a pipe for improved error detection
  execvp(args[0], args);
  exit(errno);
}

int execpane_init(void* ptr){
  int pane = *(int*)ptr;
  return tym_pane_set_env(pane);
}

int execpane(int pane, char* argv[]){
  return forkcall(&pane, execpane_init, argv);
}

int main(){
  if(tym_init()){
    perror("tym_init failed");
    return 1;
  }
  set_keyboard_size(24);
  top_pane = tym_pane_create(&top_pane_coordinates);
  if(top_pane == -1){
    perror("tym_create_pane failed");
    return 1;
  }
  bottom_pane = tym_pane_create(&bottom_pane_coordinates);
  if(bottom_pane == -1){
    perror("tym_create_pane failed");
    return 1;
  }
  int tpid = execpane(   top_pane, (char*[]){"bash", "-l", 0});
  int bpid = execpane(bottom_pane, (char*[]){"bash", 0});
  while(waitpid(tpid,0,0) == -1 && errno == EINTR);
  while(waitpid(bpid,0,0) == -1 && errno == EINTR);
  tym_shutdown();
  return 0;
}
