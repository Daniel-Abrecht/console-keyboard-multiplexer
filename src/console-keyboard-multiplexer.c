// Copyright (c) 2018 Daniel Abrecht
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <poll.h>
#include <libttymultiplex.h>
#include <libconsolekeyboard.h>

int top_pane = -1;
struct tym_super_position_rectangle top_pane_coordinates = {
  .edge[TYM_RECT_BOTTOM_RIGHT].type[TYM_P_RATIO].axis = {
    [TYM_AXIS_HORIZONTAL].value.real = 1,
    [TYM_AXIS_VERTICAL].value.real = 1,
  }
};

int bottom_pane = -1;
struct tym_super_position_rectangle bottom_pane_coordinates = {
  .edge[TYM_RECT_TOP_LEFT].type[TYM_P_RATIO].axis = {
    [TYM_AXIS_VERTICAL].value.real = 1,
  },
  .edge[TYM_RECT_BOTTOM_RIGHT].type[TYM_P_RATIO].axis = {
    [TYM_AXIS_HORIZONTAL].value.real = 1,
    [TYM_AXIS_VERTICAL].value.real = 1,
  }
};

void set_keyboard_size(struct lck_super_size size){
  TYM_RECT_POS_REF(top_pane_coordinates, CHARFIELD, TYM_BOTTOM) = -size.character;
  TYM_RECT_POS_REF(bottom_pane_coordinates, CHARFIELD, TYM_TOP) = -size.character;
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

void cleanup(void){
  tym_shutdown();
}

int forkcall(void* ptr, int(*init)(void*ptr), char* args[], int cfd){
  int ret = fork();
  if(ret == -1)
    return -1;
  if(ret)
    return ret;
  ret = (*init)(ptr);
  if(ret == -1)
    exit(errno);
  // TODO: use a pipe for improved error detection
  if(cfd > 0){
    dup2(cfd,3);
    if(cfd != 3)
      close(cfd);
  }
  execvp(args[0], args);
  exit(errno);
}

int execpane_init(void* ptr){
  int pane = *(int*)ptr;
  return tym_pane_set_env(pane);
}

int execpane(int pane, char* argv[], int cfd){
  return forkcall(&pane, execpane_init, argv, cfd);
}

int childexitnotifier = -1;

void childexit(int x){
  (void)x;
  write(childexitnotifier,"",1);
}

uint64_t bytes_to_uint64(uint8_t in[8]){
  uint64_t x = 0;
  x = (uint64_t)in[0] << 56
    | (uint64_t)in[1] << 48
    | (uint64_t)in[2] << 40
    | (uint64_t)in[3] << 32
    | (uint64_t)in[4] << 24
    | (uint64_t)in[5] << 16
    | (uint64_t)in[6] << 8
    | (uint64_t)in[7];
  return x;
}

int parse(size_t s, uint8_t b[s+1]){
  if(s < 1)
    return -1;
  enum lck_cmd cmd = *b;
  b += 1;
  s -= 1;
  switch(cmd){
    case LCK_SEND_KEY   : return tym_pane_send_special_key_by_name(TYM_PANE_FOCUS, (char*)b);
    case LCK_SEND_STRING: return tym_pane_type(TYM_PANE_FOCUS, s, (char*)b);
    case LCK_SET_HEIGHT: {
      struct lck_super_size size;
      memset(&size, 0, sizeof(size));
      if(s >= 8) size.character = bytes_to_uint64(b);
      set_keyboard_size(size);
    }; return 0;
    default: return -1;
  }
  return 0;
}

int main(int argc, char* argv[]){

  if(argc <= 1)
    argv = (char*[]){argv[0],"login",0};

  atexit(cleanup);

  // Initialise terminal multiplexer & add panes
  if(tym_init()){
    perror("tym_init failed");
    return 1;
  }
  struct lck_super_size size = {
    .character = 12
  };
  set_keyboard_size(size);
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
  tym_pane_set_flag(bottom_pane, TYM_PF_DISALLOW_FOCUS, true);
  tym_pane_set_flag(top_pane, TYM_PF_FOCUS, true);

  int sfd[2];
  if(pipe(sfd) == -1){
    perror("pipe failed");
    return 1;
  }
  childexitnotifier = sfd[1];
  signal(SIGCHLD, childexit);

  // Execute programs;
  int tpid = execpane(   top_pane, argv+1, -1);
  int cfd[2];
  if(pipe(cfd) == -1){
    perror("pipe failed");
    return 1;
  }
  fcntl(cfd[0], F_SETFL, O_NONBLOCK);
  int bpid = execpane(bottom_pane, (char*[]){"console-keyboard-basic", 0}, cfd[1]);
  close(cfd[1]);

  (void)tpid;
  (void)bpid;

  // Wait for input
  enum {
    PFD_SIGCHILD,
    PFD_KEYBOARDINPUT
  };

  struct pollfd fds[] = {
    [PFD_SIGCHILD] = {
      .fd = sfd[0],
      .events = POLLIN
    },
    [PFD_KEYBOARDINPUT] = {
      .fd = cfd[0],
      .events = POLLIN
    },
  };
  size_t nfds = sizeof(fds)/sizeof(*fds);

  int p_remaining = 0;
  int p_size = 0;
  uint8_t p_buffer[257];

  while( true ){
    int ret = poll(fds, nfds, -1);
    if( ret == -1 ){
      if( errno == EINTR )
        continue;
      perror("poll failed");
      return 1;
    }
    if(!ret)
      continue;
    if(fds[PFD_SIGCHILD].revents & POLLIN)
      break;
    if(fds[PFD_KEYBOARDINPUT].revents & POLLIN){
      unsigned char s;
      if(!p_remaining){
        if(read(cfd[0],&s,sizeof(s)) != 1)
          continue;
        p_size = s;
        p_remaining = s;
      }else{
        int n = read(cfd[0], p_buffer + (p_size - p_remaining), p_remaining);
        if(n <= 0)
          continue;
        if(n > p_remaining)
          abort();
        p_remaining -= n;
        p_buffer[p_size] = 0;
        if(!p_remaining)
          parse(p_size, p_buffer);
      }
    }
  }

  return 0;
}
