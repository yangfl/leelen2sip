#ifndef UTILS_SOCKET_H
#define UTILS_SOCKET_H

#ifdef __cplusplus
extern "C" {
#endif


int setnonblocking (int fd);

static inline int setsockopt_i (
    int socket, int level, int option_name, int on) {
  return setsockopt(socket, level, option_name, &on, sizeof(on));
}

int setsockopt_e (
  int socket, int level, int option_name,
  const void *option_value, socklen_t option_len);
int setsockopt_ie (int socket, int level, int option_name, int on);


#ifdef __cplusplus
}
#endif

#endif /* UTILS_SOCKET_H */
