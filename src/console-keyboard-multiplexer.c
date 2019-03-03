// Copyright (c) 2018 Daniel Abrecht
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <poll.h>
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

int main(){

  atexit(cleanup);

  // Initialise terminal multiplexer & add panes
  if(tym_init()){
    perror("tym_init failed");
    return 1;
  }
  set_keyboard_size(12);
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
  int tpid = execpane(   top_pane, (char*[]){"login", 0}, -1);
  int cfd[2];
  if(pipe(cfd) == -1){
    perror("pipe failed");
    return 1;
  }
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
      int c;
      int mfd = tym_pane_get_masterfd(top_pane);
      while(read(cfd[0],&c,1) == 1){
        write(mfd, &c, 1);
      }
    }
  }

  signal(SIGCHLD, SIG_IGN);
  while(waitpid(-1, 0, WNOHANG) > 0); // In case a child exited before SIGCHLD was set to SIG_IGN

  return 0;
}
