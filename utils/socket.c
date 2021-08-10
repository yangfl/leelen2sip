#include <fcntl.h>
#include <stdio.h>
#include <sys/socket.h>

#include "macro.h"
#include "socket.h"


extern inline int setsockopt_i (int socket, int level, int option_name, int on);


int setnonblocking (int fd) {
  int old_option = fcntl(fd, F_GETFL);
  fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
  return old_option;
}


int setsockopt_e (
    int socket, int level, int option_name,
    const void *option_value, socklen_t option_len) {
  int ret = setsockopt(socket, level, option_name, option_value, option_len);
  should (ret == 0) otherwise {
    perror("setsockopt");
  }
  return ret;
}


int setsockopt_ie (int socket, int level, int option_name, int on) {
  int ret = setsockopt_i(socket, level, option_name, on);
  should (ret == 0) otherwise {
    perror("setsockopt");
  }
  return ret;
}
