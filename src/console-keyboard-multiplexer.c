// Copyright (c) 2018 Daniel Abrecht
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <execinfo.h>
#include <pwd.h>
#include <grp.h>
#include <utmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <poll.h>
#include <getopt.h>
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
      TYM_U_PERROR(TYM_LOG_ERROR, "tym_resize_pane failed");
    }
  }
  if(bottom_pane != -1){
    if( tym_pane_resize(bottom_pane, &bottom_pane_coordinates) == -1){
      TYM_U_PERROR(TYM_LOG_ERROR, "tym_resize_pane failed");
    }
  }
}

struct user_group {
  uid_t user;
  gid_t group;
  size_t supplementary_group_count;
  gid_t* supplementary_group_list;
  bool ignore;
  bool option;
};

struct ckm_args {
  bool help;
  bool retain_pid;
  int print_fd;
  struct user_group main_user;
  struct user_group keyboard_user;
  struct user_group program_user;
  bool has_keyboard;
  char** keyboard;
};

#define NOBODY  65534
#define NOGROUP 65534

static struct ckm_args args = {
  .help = false,
  .print_fd = -1,
  .keyboard = (char*[]){(char[]){"console-keyboard"},0},
  .main_user     = {NOBODY, NOGROUP, false},
  .keyboard_user = {NOBODY, NOGROUP, false},
  .program_user  = {NOBODY, NOGROUP, false},
};


void cleanup(void){
  tym_shutdown();
}

typedef int(*execpane_setup_t)(void*ptr);

int blockreadchar(int fd){
  while(true){
    unsigned char c = 0;
    int res = read(fd, &c, 1);
    if(res == -1 && errno == EINTR)
      continue;
    if(res == -1)
      return -1;
    if(res == 0)
      return 256;
    return c;
  }
}

int execpane(void* ptr, size_t setup_count, execpane_setup_t setup[setup_count], char* args[], int cfd, bool inverse){
  pid_t newpid = 0;
  pid_t oldpid = getpid();
  pid_t result = 0;

  int endpipe[2];
  int sync_ab[2];
  int sync_ba[2];

  if(pipe(endpipe) == -1){
    TYM_U_PERROR(TYM_LOG_ERROR, "pipe failed");
    return -1;
  }

  if(fcntl(endpipe[0], F_SETFD, FD_CLOEXEC) == -1){
    TYM_U_PERROR(TYM_LOG_ERROR, "fcntl(F_SETFD, FD_CLOEXEC) failed");
    close(endpipe[0]);
    close(endpipe[1]);
    return -1;
  }

  if(fcntl(endpipe[1], F_SETFD, FD_CLOEXEC) == -1){
    TYM_U_PERROR(TYM_LOG_ERROR, "fcntl(F_SETFD, FD_CLOEXEC) failed");
    close(endpipe[0]);
    close(endpipe[1]);
    return -1;
  }

  if(pipe(sync_ab) == -1){
    TYM_U_PERROR(TYM_LOG_ERROR, "pipe failed");
    close(endpipe[0]);
    close(endpipe[1]);
    return -1;
  }

  if(pipe(sync_ba) == -1){
    TYM_U_PERROR(TYM_LOG_ERROR, "pipe failed");
    close(endpipe[0]);
    close(endpipe[1]);
    close(sync_ab[0]);
    close(sync_ab[1]);
    return -1;
  }

  newpid = fork();
  if(newpid == -1)
    return -1;

  if(inverse){
    if(!newpid)
      result = oldpid;
  }else{
    if(newpid)
      result = newpid;
  }

  if(result){
    close(endpipe[1]);
    close(sync_ba[1]);
    close(sync_ab[0]);
    while( write(sync_ab[1], (char[]){1}, 1) == -1 && errno == EINTR );
    size_t i;
    for(i=1; i<setup_count; i += 2){
      int ret = blockreadchar(sync_ba[0]);
      if(ret != 1)
        goto getresult_oldproc;
      execpane_setup_t sfunc = setup[i];
      if(sfunc){
        int ret = (*sfunc)(ptr);
        if(ret == -1)
          goto getresult_oldproc;
      }
      while( write(sync_ab[1], (char[]){1}, 1) == -1 && errno == EINTR );
    }
    int ret = blockreadchar(sync_ba[0]);
    if(ret != 1)
      goto getresult_oldproc;
    if(i == setup_count)
      while( write(sync_ab[1], (char[]){1}, 1) == -1 && errno == EINTR );
    close(sync_ba[0]);
    close(sync_ab[1]);
    goto getresult_oldproc;
  }

  close(endpipe[0]);
  close(sync_ba[0]);
  close(sync_ab[1]);

  size_t i = 0;
  for(i=0; i<setup_count; i += 2){
    int ret = blockreadchar(sync_ab[0]);
    if(ret != 1)
      goto error_newproc;
    execpane_setup_t sfunc = setup[i];
    if(sfunc){
      int ret = (*sfunc)(ptr);
      if(ret == -1)
        goto error_newproc;
    }
    while( write(sync_ba[1], (char[]){1}, 1) == -1 && errno == EINTR );
  }
  int ret = blockreadchar(sync_ab[0]);
  if(ret != 1)
    goto error_newproc;
  if(i == setup_count)
    while( write(sync_ba[1], (char[]){1}, 1) == -1 && errno == EINTR );
  close(sync_ba[1]);
  close(sync_ab[0]);

  if(cfd > 0){
    dup2(cfd, 3);
    if(cfd != 3)
      close(cfd);
  }

  execvp(args[0], args);
error_newproc:
  if(!errno)
    errno = 1;
  close(sync_ba[1]);
  while( write(endpipe[1], (char[]){errno}, 1) == -1 && errno == EINTR );
  close(endpipe[1]);
  close(sync_ab[0]);
  exit(1);
getresult_oldproc:;
  close(sync_ab[1]);
  int ch = blockreadchar(endpipe[0]);
  close(endpipe[0]);
  close(sync_ba[0]);
  if(ch == -1)
    ch = errno;
  if(ch == 0)
    ch = 1;
  if(ch != 256){
    errno = ch;
    return -1;
  }
  return result;
}

int execpane_init(void* ptr){
  int pane = *(int*)ptr;
  if(tym_pane_set_env(pane) == -1)
    return -1;
  struct user_group ug = pane == top_pane ? args.program_user : args.keyboard_user;
  if(!ug.ignore){
    bool fatal = getpid() == 0;
    if(setgid(ug.group) == -1){
      TYM_U_PERROR(fatal ? TYM_LOG_ERROR : TYM_LOG_WARN, "setgid failed");
      if(fatal)
        return -1;
    }
    if(setgroups(ug.supplementary_group_count, ug.supplementary_group_list) == -1){
      TYM_U_PERROR(fatal ? TYM_LOG_ERROR : TYM_LOG_WARN, "setgroups failed");
      if(fatal)
        return -1;
    }
    if(setuid(ug.user) == -1){
      TYM_U_PERROR(fatal ? TYM_LOG_ERROR : TYM_LOG_WARN, "setuid failed");
      if(fatal)
        return -1;
    }
  }
  return 0;
}

bool is_session_leader = false;
bool waithup = false;
bool gothup = false;
void hupwaiter(int signo){
  (void)signo;
  gothup = true;
  waithup = false;
}

int execpane_takeover_tty(void* ptr){
  (void)ptr;
  if(getpid() != getpgid(0)){
    if(setpgid(0,0) == -1){
      TYM_U_PERROR(TYM_LOG_ERROR, "setpgid failed: %s");
      return -1;
    }
  }
  signal(SIGTTOU, SIG_IGN);
  signal(SIGTTIN, SIG_IGN);
  if(tcsetpgrp(STDIN_FILENO,getpid()) == -1){
    TYM_U_PERROR(TYM_LOG_ERROR, "tcsetpgrp failed: %s");
    return -1;
  }
  if(is_session_leader){
    waithup = true;
    signal(SIGHUP, hupwaiter);
  }
  return 0;
}

// We may have lost our controling terminal in the mean time. Reclaim it if so.
int execpane_takeover_tty2(void* ptr){
  // Wait about 2 seconds for the sighup signal, if it didn't arrive already
  for(int i=0; waithup && i < 2 * 10; i++)
    usleep(100000);
  if(gothup){
    gothup = false;
    signal(SIGHUP, SIG_DFL); // reset signal handler
  }
  if(open("/dev/tty", O_RDONLY) == -1){
    if(errno != ENXIO){
      TYM_U_PERROR(TYM_LOG_ERROR, "open failed: %s");
      return -1;
    }
    // OK, we lost the tty. This means the original process, whose content shall
    // be redirected to the top pane, had to give up the current terminal to gain
    // a new one, which caused this process to loose the controling terminal (and to get a sighup).
    // We need to become the new session leader. But to do so, we first need to change our process
    // group to something else than ours, or setsid will fail.
    int waitpipe[2];
    if(pipe(waitpipe) == -1){
      TYM_U_PERROR(TYM_LOG_ERROR, "pipe failed: %s");
      return -1;
    }
    int ret = fork();
    if(ret == -1){
      close(waitpipe[0]);
      close(waitpipe[1]);
      TYM_U_PERROR(TYM_LOG_ERROR, "fork failed: %s");
      return -1;
    }
    if(!ret){
      close(waitpipe[0]);
      setpgid(0,0); // Make a new process group
      close(waitpipe[1]); // signal old process
      while(true)
        pause();
      exit(1);
    }else{
      close(waitpipe[1]);
      blockreadchar(waitpipe[0]); // wait until the other process is ready
      close(waitpipe[0]);
      if(setpgid(0,ret) == -1){ // Switch to new process group of other process
        TYM_U_PERROR(TYM_LOG_ERROR, "setpgid failed: %s");
        kill(ret, SIGKILL);
        return -1;
      }
      kill(ret, SIGKILL);
      while(waitpid(ret,0,0) == -1 && errno == EINTR);
    }
    if(setsid() == -1){ // Now, getpid() != getpgid(0), now setsid should work.
      TYM_U_PERROR(TYM_LOG_ERROR, "setsid failed: %s");
      return -1;
    }
    if(ioctl(STDIN_FILENO, TIOCSCTTY, 0) == -1){
      TYM_U_PERROR(TYM_LOG_ERROR, "ioctl(STDIN_FILENO, TIOCSCTTY, 0) failed: %s");
      return -1;
    }
  }
  return execpane_takeover_tty(ptr);
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

void trim(char** pstr){
  if(!pstr || !*pstr)
    return;
  char* str = *pstr;
  while(*str && isspace(*str))
    str += 1;
  char* end = str + strlen(str);
  while(str < end && isspace(end[-1]))
    *--end = 0;
  *pstr = str;
}

int parse_user(struct user_group* ug, char* user){
  char* group = strstr(user, ":");
  if(group) *(group++) = 0;
  trim(&user);
  trim(&group);
  if(!user){
    errno = EINVAL;
    return -1;
  }
  if(!*user && group && !*group){
    ug->ignore = true;
    return 0;
  }
  struct passwd* pu = 0;
  if(*user){
    errno = 0;
    char* end = 0;
    long long uid = strtoll(user, &end, 10);
    if(uid < 0 || (uid_t)uid != uid || !end || *end || errno)
      uid = -1;
    if(uid == -1){
      pu = getpwnam(user);
      if(!pu)
        return -1;
      uid = pu->pw_uid;
    }else{
      pu = getpwuid(uid);
    }
    ug->user = uid;
    if(pu && group && !*group)
      ug->group = pu->pw_gid;
  }
  if(group && *group){
    errno = 0;
    char* end = 0;
    long long gid = strtoll(group, &end, 10);
    if(gid < 0 || (gid_t)gid != gid || !end || *end || errno)
      gid = -1;
    if(gid == -1){
      struct group* pg = getgrnam(group);
      if(!pg)
        return -1;
      ug->group = pg->gr_gid;
    }else{
      ug->group = gid;
    }
  }
  // Let's not give nobody any groups. It's supposed to be the user that can do basically nothing after all.
  if(pu && ug->user != NOBODY){
    // If any of this fails, we'll just remove all supplementary groups
    int ngroups = 0;
    if(getgrouplist(pu->pw_name, ug->group, 0, &ngroups) == -1){
      if(ngroups < 0){
        TYM_U_PERROR(TYM_LOG_WARN, "getgrouplist: invalid group count");
        return 0;
      }
      gid_t* gid_list = malloc(sizeof(gid_t) * ngroups);
      if(!gid_list){
        TYM_U_PERROR(TYM_LOG_WARN, "malloc falsed");
        return 0;
      }
      if(getgrouplist(pu->pw_name, ug->group, gid_list, &ngroups) == -1){
        free(gid_list);
        TYM_U_PERROR(TYM_LOG_WARN, "getgrouplist falsed");
        return 0;
      }
      ug->supplementary_group_list = gid_list;
      ug->supplementary_group_count = ngroups;
    }
  }
  return 0;
}

int parseopts(int* pargc, char*** pargv){
  int argc = *pargc;
  char** argv = *pargv;

  int nargs = 0;
  for(int i=1; i<argc; i++){
    if(strcmp("--", argv[i]))
      continue;
    nargs = i;
    break;
  }

  int opt_argc = nargs ? nargs : argc;

  static struct option long_options[] = {
      {"help"          , no_argument, 0,  'h'},
      {"retain-pid"    , no_argument, 0,  'r'},
      {"print-env"     , required_argument, 0,  'p'},
      {"user"          , required_argument, 0,  'u'},
      {"keyboard-user" , required_argument, 0,  'v'},
      {"program-user"  , required_argument, 0,  'w'},
      {"keyboard"      , no_argument, 0,  'k'},
      {0,         0,                 0,  0 }
  };

  int c;
  while((c = getopt_long(opt_argc, argv, "hrlkp:u:v:w:", long_options, 0)) != -1){
    switch (c){
      case 'h': args.help = true; return 0;
      case 'r': args.retain_pid = true; break;
      case 'k': {
        args.has_keyboard = true;
      } break;
      case 'p': {
        char* end = 0;
        long fd = strtol(optarg, &end, 10);
        if(fd < 3 || (int)fd != fd || *end){
          errno = EINVAL;
          return -1;
        }
        args.print_fd = fd;
      } break;
      case 'u': {
        args.main_user.option = true;
        if(parse_user(&args.main_user, optarg) == -1)
          return -1;
      } break;
      case 'v': {
        args.keyboard_user.option = true;
        if(parse_user(&args.keyboard_user, optarg) == -1)
          return -1;
      } break;
      case 'w': {
        args.program_user.option = true;
        if(parse_user(&args.program_user, optarg) == -1)
          return -1;
      } break;
      case '?':
        if(isprint(optopt))
          fprintf(stderr, "Invalid option `-%c'.\n", optopt);
        else
          fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
        errno = EINVAL;
        return -1;
      default: return -1;
    }
  }

  if(optind <= 0)
    return 0;

  if(opt_argc != optind){
    errno = EINVAL;
    return -1;
  }

  if(0 <= args.print_fd && (args.retain_pid || args.program_user.option)){
    errno = EINVAL;
    return -1;
  }

  if(nargs){
    argv[nargs] = *argv;
    argc -= opt_argc;
    argv += opt_argc;
  }

  if(args.has_keyboard){
    int nargs = 0;
    for(int i=1; i<argc; i++){
      if(strcmp("--", argv[i]))
        continue;
      nargs = i;
      break;
    }
    if(nargs){
      argv[nargs] = *argv;
      memmove(argv, argv+1, sizeof(*argv)*(nargs-1));
      argv[nargs-1] = 0;
      args.keyboard = argv;
      argc -= nargs;
      argv += nargs;
    }else if(argv[0]){
      args.keyboard = argv+1;
      argv = *pargv;
      argc = 1;
      argv[1] = 0;
    }
  }

  if(args.has_keyboard && (!args.keyboard || !*args.keyboard)){
    errno = EINVAL;
    return -1;
  }

  if(0 <= args.print_fd && argc > 1){
    errno = EINVAL;
    return -1;
  }

  *pargc = argc;
  *pargv = argv;
  **pargv = *argv;

  return 0;
}

void usage(const char* name){
  printf(
    "\nUsage: %s [options] [-k -- [keyboard] [args]] -- program [args]\n"
    "\n"
    "If no program is specified, the login program is used.\n"
    "If no keyboard is specified, the console-keyboard program is used, which is usually a symlink to the prefered console keyboard.\n"
    "\n"
    "Optional options:\n"
    "  -h             This help text\n"
    "  -r             Retain the original pid. The program specified shall replace the original console-keyboard-multiplexer process, which will continue running in a fork. console-keyboard-multiplexer becoms a child of the specified program, unless -d is specified. The main use case is executing an init process, which must retain PID 1.\n"
    "  -p fd          Instead of executing the specified program or the login program, print some environment variables to file descriptor fd. The smallest allowed fd is 3. The name of the pts is exported as environment variable TM_E_PTS. This is useful for redirecting the input and output of an already running script or program & give it a console keyboard that way. For this, the controlling terminal of said script/program also has to be changed, a functionality current shells lack.\n"
    "  -u user:group  Specify the user and group this program shall run as. You can use decimal numbers or usernames. Usernames which happen to be numbers are not suported. The format is \"user\" to only specify a \"user:\" to also set the users group, \"user:group\" to specify both, and :group to only set the group. The default user and group are nobody:nogroup. A failure to change the user or group is only fatal for root.\n"
    "  -v user:group  Specify the user and group the keyboard shall run as. You can use decimal numbers or usernames. Usernames which happen to be numbers are not suported. The format is \"user\" to only specify a \"user:\" to also set the users group, \"user:group\" to specify both, and :group to only set the group. The default user and group are nobody:nogroup. A failure to change the user or group is only fatal for root.\n"
    "  -w user:group  Specify the user and group the user specified porigram shall run as. You can use decimal numbers or usernames. Usernames which happen to be numbers are not suported. The format is \"user\" to only specify a \"user:\" to also set the users group, \"user:group\" to specify both, and :group to only set the group. The default user and group are nobody:nogroup. A failure to change the user or group is only fatal for root.\n"
    "  -k             The console keyboard program to be started should be specified\n"
    "\n"
  , name);
}

int childexitnotifier = -1;
int childs[2] = {-1,-1};

void childexit(int x){
  (void)x;
  while(true){
    int status;
    pid_t pid = waitpid(-1, &status, WNOHANG);
    if(pid <= 0)
      break;
    for(int i=0; i<2; i++){
      if(pid == childs[i]){
        write(childexitnotifier,"",1);
        childs[i] = -1;
      }
    }
  }
  for(int i=0; i<2; i++){
    if(childs[i] != -1){
      if(kill(childs[i], 0) == -1 && errno == ESRCH){
        write(childexitnotifier,"",1);
        childs[i] = -1;
      }
    }
  }
}

static int do_print_args(int pane, void* ptr, size_t count, const char* env[count][2]){
  (void)pane;
  (void)ptr;
  for(size_t i=0; i<count; i++)
    if(dprintf(args.print_fd, "%s=%s\n", env[i][0], env[i][1]) == -1)
      return 1;
  return 0;
}

int main(int argc, char* argv[]){

  is_session_leader = getpid() == getsid(0);

  if(parseopts(&argc, &argv) == -1 || argc <= 1){
    TYM_U_PERROR(TYM_LOG_FATAL, "parseopts failed");
    usage(*argv);
    return 1;
  }

  if(args.help){
    usage(*argv);
    return 0;
  }

  if(args.print_fd >= 0){
    if(fcntl(args.print_fd, F_SETFD, FD_CLOEXEC) == -1){
      TYM_U_PERROR(TYM_LOG_FATAL, "fcntl(args.print_fd, F_SETFD, FD_CLOEXEC) failed");
      return 1;
    }
  }

  atexit(cleanup);

  // Initialise terminal multiplexer & add panes
  if(tym_init()){
    TYM_U_PERROR(TYM_LOG_FATAL, "tym_init failed");
    return 1;
  }
  struct lck_super_size size = {
    .character = 12
  };
  set_keyboard_size(size);
  top_pane = tym_pane_create(&top_pane_coordinates);
  if(top_pane == -1){
    TYM_U_PERROR(TYM_LOG_FATAL, "tym_create_pane failed");
    return 1;
  }
  bottom_pane = tym_pane_create(&bottom_pane_coordinates);
  if(bottom_pane == -1){
    TYM_U_PERROR(TYM_LOG_FATAL, "tym_create_pane failed");
    return 1;
  }
  tym_pane_set_flag(bottom_pane, TYM_PF_DISALLOW_FOCUS, true);
  tym_pane_set_flag(top_pane, TYM_PF_FOCUS, true);

  int sfd[2];
  if(pipe(sfd) == -1){
    TYM_U_PERROR(TYM_LOG_FATAL, "pipe failed");
    return 1;
  }
  childexitnotifier = sfd[1];
  signal(SIGCHLD, childexit);

  // Freeze libttymultiplex because of upcoming forks
  if(tym_freeze() == -1){
    TYM_U_PERROR(TYM_LOG_FATAL, "tym_freeze failed");
    return 1;
  }

  // Execute programs
  if(args.print_fd >= 0){
    int ptsfd = tym_pane_get_slavefd(top_pane);
    const char* ptsdev = ttyname(ptsfd);
    if(!ptsdev || !*ptsdev)
      return 1;
    if(dprintf(args.print_fd, "TM_E_TTY=%s\n", ptsdev) == -1)
      return 1;
    if(tym_pane_get_default_env_vars(top_pane, 0, do_print_args) == -1)
      return 1;
    close(args.print_fd);
  }else{
    if(args.retain_pid){
      if((childs[0]=execpane(&top_pane, 4, (execpane_setup_t[]){0,execpane_takeover_tty,execpane_init,execpane_takeover_tty2}, argv+1, -1, true)) == -1)
        return 1;
    }else{
      if((childs[0]=execpane(&top_pane, 1, (execpane_setup_t[]){execpane_init}, argv+1, -1, false)) == -1)
        return 1;
    }
  }
  int cfd[2];
  if(pipe(cfd) == -1){
    TYM_U_PERROR(TYM_LOG_FATAL, "pipe failed");
    return 1;
  }
  fcntl(cfd[0], F_SETFL, O_NONBLOCK);
  if((childs[1]=execpane(&bottom_pane, 1, (execpane_setup_t[]){execpane_init}, args.keyboard, cfd[1], false)) == -1)
    return -1;
  close(cfd[1]);

  if(!args.main_user.ignore){
    bool fatal = getpid() == 0;
    if(setgid(args.main_user.group) == -1){
      TYM_U_PERROR(fatal ? TYM_LOG_FATAL : TYM_LOG_WARN, "setgid failed");
      if(fatal)
        return 1;
    }
    if(setgroups(args.main_user.supplementary_group_count, args.main_user.supplementary_group_list) == -1){
      TYM_U_PERROR(fatal ? TYM_LOG_FATAL : TYM_LOG_WARN, "setgroups failed");
      if(fatal)
        return 1;
    }
    if(setuid(args.main_user.user) == -1){
      TYM_U_PERROR(fatal ? TYM_LOG_FATAL : TYM_LOG_WARN, "setuid failed");
      if(fatal)
        return 1;
    }
  }

  if(args.retain_pid && prctl(PR_SET_PDEATHSIG, SIGCHLD) == -1){
    TYM_U_PERROR(TYM_LOG_FATAL, "prctl(PR_SET_PDEATHSIG, SIGCHLD) failed");
    return 1;
  }

  // Resume libttymultiplex
  if(tym_init()){
    TYM_U_PERROR(TYM_LOG_FATAL, "tym_init failed");
    return 1;
  }

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
      TYM_U_PERROR(TYM_LOG_FATAL, "poll failed");
      return 1;
    }
    if(!ret)
      continue;

    if(fds[PFD_SIGCHILD].revents & POLLIN){
      bool out = false;
      while(true){
        unsigned char c = 0;
        int r = read(fds[PFD_SIGCHILD].fd, &c, 1);
        if(r == -1 && errno == EINTR)
          continue;
        if(r == 0 || (r == 1 && c == 0)){
          out = true;
          break;
        }
        break;
      }
      if(out)
        break;
    }

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
