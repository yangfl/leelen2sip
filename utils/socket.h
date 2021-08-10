#ifndef UTILS_SOCKET_H
#define UTILS_SOCKET_H


int setnonblocking (int fd);

inline int setsockopt_i (int socket, int level, int option_name, int on) {
  return setsockopt(socket, level, option_name, &on, sizeof(on));
}

int setsockopt_e (
  int socket, int level, int option_name,
  const void *option_value, socklen_t option_len);
int setsockopt_ie (int socket, int level, int option_name, int on);


#endif /* UTILS_SOCKET_H */
