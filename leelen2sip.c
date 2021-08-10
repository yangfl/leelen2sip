#include <errno.h>
#include <getopt.h>
#include <regex.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <inet46i/endian.h>
#include <inet46i/in46.h>
#include <inet46i/pbr.h>
#include <inet46i/server.h>
#include <inet46i/socket46.h>
#include <osip2/osip.h>

#include "utils/macro.h"
#include "utils/arg.h"
#include "utils/log.h"
#include "utils/socket.h"
#include "leelen/config.h"
#include "leelen/discovery.h"
#include "leelen/protocol.h"
#include "sip/ua.h"
#include "leelen2sip.h"


volatile bool leelen2sip_shutdown = false;


static void shutdown_leelen2sip (int sig) {
  (void) sig;

  if (!leelen2sip_shutdown) {
    LOGGER(LEELEN2SIP_NAME, LOG_LEVEL_INFO, "Shutting down " LEELEN2SIP_NAME);
    leelen2sip_shutdown = true;
    // wait poll to break
  }
}


static int LeelenDiscovery_receive_pbr (
    struct LeelenDiscovery *self, char *buf, int buflen, int socket,
    struct Socket5Tuple *conn, void *spec_dst) {
  buf[buflen] = '\0';
  int ret = LeelenDiscovery_receive(
    self, buf, socket, &conn->src.sock, spec_dst, conn->dst.sa_scope_id);
  should (ret > 0) otherwise {
    if (ret == 0) {
      LOGGER_PERROR(LEELEN_NAME, LOG_LEVEL_INFO,
                    "Discovery message '%s' results in nothing", buf);
    } else {
      LOGGER_PERROR(LEELEN_NAME, LOG_LEVEL_INFO,
                    "Error when processing discovery message '%s'", buf);
    }
  }
  return ret;
}


static int start_leelen2sip (
    struct LeelenDiscovery *device, struct LeelenSIP *sip,
    union sockaddr_in46 *leelen_addr, union sockaddr_in46 *sip_addr,
    bool daemonize) {
  struct SocketPolicyRoute sip_routes[2];

  // check ports
  int ind_shared_socket = -1;
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < i; j++) {
      should (device->config->ports[i] != device->config->ports[j]) otherwise {
        fprintf(stderr, "error: LEELEN ports conflicted\n");
        return 255;
      }
    }
    continue_if (device->config->ports[i] != ntohs(sip_addr->sa_port));

    // prepare SocketPolicyRoute
    memset(sip_routes, 0, sizeof(sip_routes));
    in64_set(leelen_addr->sa_family, &sip_routes[0].dst.network6,
             sockaddr_addr(&leelen_addr->sock));
    in64_set(sip_addr->sa_family, &sip_routes[1].dst.network6,
             sockaddr_addr(&sip_addr->sock));

    // when we should do pbr...
    bool leelen_host_unspecified =
      IN64_IS_ADDR_UNSPECIFIED(&sip_routes[0].dst.network6);
    bool sip_host_unspecified =
      IN64_IS_ADDR_UNSPECIFIED(&sip_routes[1].dst.network6);
    bool leelen_sip_host_same = memcmp(
      &sip_routes[0].dst.network6, &sip_routes[1].dst.network6,
      sizeof(struct in6_addr)) == 0;
    if (leelen_host_unspecified || sip_host_unspecified ||
        leelen_sip_host_same) {
      should (!leelen_sip_host_same ||
              sip_addr->sa_scope_id != leelen_addr->sa_scope_id) otherwise {
        fprintf(stderr, "error: LEELEN and SIP listen to the same port on the "
                        "same address/interface!\n");
        return 255;
      }
      ind_shared_socket = i;
      LOGGER(LEELEN2SIP_NAME, LOG_LEVEL_INFO, "Reuse port %d",
             device->config->ports[i]);
      if (!leelen_host_unspecified) {
        sip_routes[0].dst.prefix = -1;
      }
      if (!sip_host_unspecified) {
        sip_routes[1].dst.prefix = -1;
      }
    }
  }

  // set up sip domain
  sip->domain = sip_addr->sa_family;
  // open sockets
  int sockets[5];
  int n_socket = -1;
  for (int i = 0; i < 3; i++) {
    union sockaddr_in46 *leelen_listen = leelen_addr;
    union sockaddr_in46 leelen_addr_local;
    // 0 is discovery
    if (i == 0 || i == ind_shared_socket) {
      leelen_addr_local.sa_family = i == ind_shared_socket ?
        max(leelen_addr->sa_family, sip_addr->sa_family) :
        leelen_addr->sa_family;
      leelen_addr_local.sa_scope_id = i == ind_shared_socket ?
        0 : leelen_addr->sa_scope_id;
      if (leelen_addr_local.sa_family == AF_INET) {
        memset(sockaddr_addr(&leelen_addr_local.sock), 0,
               sizeof(struct in_addr));
      } else {
        memset(sockaddr_addr(&leelen_addr_local.sock), 0,
               sizeof(struct in6_addr));
        leelen_addr_local.v6.sin6_flowinfo = 0;
      }
      leelen_listen = &leelen_addr_local;
    }
    leelen_listen->sa_port = htons(device->config->ports[i]);
    // set up discovery domain
    if (i == 0) {
      device->domain = leelen_listen->sa_family;
    }
    // set up sip domain
    if (i == ind_shared_socket) {
      sip->domain = leelen_listen->sa_family;
    }

    sockets[i] = bindport(leelen_listen, SOCK_DGRAM);
    should (sockets[i] >= 0) otherwise {
      fprintf(stderr, "cannot open port %d for protocol %s: ",
              device->config->ports[i], leelen_protocol_name[i]);
      perror("");
      goto fail;
    }

    n_socket = i;
    setnonblocking(sockets[i]);
    setsockopt_ie(sockets[i], SOL_SOCKET, SO_REUSEADDR, 1);

    if (i == 0 || i == ind_shared_socket) {
      int enabled = 1;
      should (setsockopt(
          sockets[i],
          leelen_listen->sa_family == AF_INET ? IPPROTO_IP : IPPROTO_IPV6,
          leelen_listen->sa_family == AF_INET ? IP_PKTINFO : IPV6_RECVPKTINFO,
          &enabled, sizeof(enabled)) >= 0) otherwise {
        perror("setsockopt");
        goto fail;
      }
    }
  }
  if (ind_shared_socket < 0) {
    sockets[3] = bindport(sip_addr, SOCK_DGRAM);
    should (sockets[3] >= 0) otherwise {
      fprintf(stderr, "cannot open port %d for protocol %s: ",
              ntohs(sip_addr->sa_port), "standard SIP");
      perror("");
      goto fail;
    }

    n_socket = 3;
    setnonblocking(sockets[3]);
    setsockopt_ie(sockets[3], SOL_SOCKET, SO_REUSEADDR, 1);
  }
  n_socket++;

  // set up LEELEN discovery
  if likely (device->config->desc == NULL) {
    ((struct LeelenConfig *) device->config)->desc =
      strdup(LEELEN2SIP_DEVICE_DESC);
    should (device->config->desc != NULL) otherwise {
      perror("strdup");
      goto fail;
    }
  }
  if likely (device->report_addr[0] == '\0') {
    LeelenDiscovery_set_report_addr(
      device, leelen_addr->sa_family, sockaddr_addr(&leelen_addr->sock));
  }
  device->socket = sockets[0];
  LOGGER(
    LEELEN2SIP_NAME, LOG_LEVEL_INFO, "Device desc: " LEELEN_DISCOVERY_FORMAT,
    device->report_addr[0] == '\0' ? "<unspecified>" : device->report_addr,
    device->config->type, device->config->desc);

  // initialize SIP convertor
  sip->socket = sockets[ind_shared_socket < 0 ? 3 : ind_shared_socket];

  // prepare cbs
  struct SocketPBR device_pbr = {
    .func = LeelenDiscovery_receive_pbr,
    .context = device,
    .single_route = true,
    .dst_required = true,
    .nothing_ret = 1,
    .maxlen = LEELEN_MAX_MESSAGE_LENGTH,
    .dst_port = htons(device->config->discovery),
  };
  struct pollservercb cbs[] = {
    {sockets[0], LeelenDiscovery_receive_pbr, &device_pbr,
     POLLSERVER_FUNC_SOCKET_CONTEXT},
    {sockets[1], pollserver_recvall},
    {sockets[2], pollserver_recvall},
    {sockets[3], LeelenSIP_receive, &sip},
    {0}
  };
  struct SocketPBR sip_pbr;
  if (ind_shared_socket >= 0) {
    sip_routes[0].dst.scope_id = leelen_addr->sa_scope_id;
    sip_routes[0].func = cbs[ind_shared_socket].func;
    sip_routes[0].context = &device;
    sip_routes[1].dst.scope_id = sip_addr->sa_scope_id;
    sip_routes[1].func = LeelenSIP_receive;
    sip_routes[1].context = &sip;

    sip_pbr.routes = sip_routes;
    sip_pbr.single_route = false;
    sip_pbr.dst_required = true;
    sip_pbr.nothing_ret = 1;
    sip_pbr.maxlen =
      max(LEELEN_MAX_MESSAGE_LENGTH, LEELEN2SIP_MAX_MESSAGE_LENGTH);
    sip_pbr.dst_port = sip_addr->sa_port;

    cbs[ind_shared_socket].func = socketpbr_read;
    cbs[ind_shared_socket].context = &sip_pbr;
    cbs[ind_shared_socket].type = POLLSERVER_FUNC_SOCKET_CONTEXT;
  }
  cbs[0].func = socketpbr_read;

  // daemonize
  if (daemonize) {
    should (daemon(0, 0) == 0) otherwise {
      perror("daemon");
      goto fail;
    }
  }

  // main loop
  LOGGER(LEELEN2SIP_NAME, LOG_LEVEL_MESSAGE, "Start " LEELEN2SIP_NAME);
  signal(SIGINT, shutdown_leelen2sip);
  pollserver(cbs, 3 + (ind_shared_socket < 0), -1, NULL);

  // end
  if (!leelen2sip_shutdown) {
    leelen2sip_shutdown = true;
  }

  int ret = 0;
  if (0) {
fail:
    ret = 1;
  }
  for (int i = 0; i < n_socket; i++) {
    close(sockets[i]);
  }
  return ret;
}


static void usage (const char progname[]) {
  fprintf(stderr, "Usage: %s [OPTIONS]... [<interface>] [<number>]\n", progname);
  fprintf(stderr,
"Convert LEELEN Video Intercom System to standard SIP protocol\n"
"\n"
"  <interface>        interface or address for LEELEN protocols\n"
"  <number>           this phone number (usually XXXX-XXXX[-X], as regex)\n"
"  -i, --interface <interface>\n"
"                     interface or address for standard SIP (default: [::])\n"
"  -p, --port <port>  standard SIP port (default: " STR(LEELEN2SIP_PORT) ")\n"
"  -D, --daemonize    daemonize (run in background)\n"
"  -d, --debug        enables debugging messages\n"
"  -h, --help         prints this help text\n"
"\n");
  fprintf(stderr,
"Advanced options:\n"
"  --report-addr <address>\n"
"             override address for reporting (default: extract from LEELEN\n"
"             listening interface)\n"
"             WARNING: this option does not check for IP validness\n"
"  --ua <ua>  SIP user agent (default: " LEELEN2SIP_NAME " "
                                         LEELEN2SIP_VERSION ")\n"
"\n");
  fprintf(stderr,
"LEELEN SIP options:\n"
"WARNING: Change these options may lead to incompatibility!\n"
"  --desc <desc>       device description (default:\n"
"                      " LEELEN2SIP_DEVICE_DESC " )\n"
"  --type <int>        device type (default: %d)\n"
"  --audio <port>      audio port (default: " STR(LEELEN_AUDIO_PORT) ")\n"
"  --video <port>      video port (default: " STR(LEELEN_VIDEO_PORT) ")\n"
"  --discovery <port>  discovery port (default: " STR(LEELEN_DISCOVERY_PORT) ")\n"
"  --leelen <port>     SIP port (default: " STR(LEELEN_SIP_PORT) ")\n"
"  --control <port>    control port (default: " STR(LEELEN_CONRTOL_PORT) ")\n",
    LEELEN_DEVICE_INDOOR_STATION);
}


int main (int argc, char *argv[]) {
  should (parser_init() == OSIP_SUCCESS) otherwise {
    fprintf(stderr, "error: cannot initialize oSIP\n");
    return EXIT_FAILURE;
  }

  bool daemonize = false;
  bool force_v6 = false;
  union sockaddr_in46 sip_addr = {
    .sa_family = AF_UNSPEC,
    .sa_port = lhtons(LEELEN2SIP_PORT),
  };
  union sockaddr_in46 leelen_addr = {
    .sa_family = AF_UNSPEC,
  };
  struct LeelenConfig config;
  LeelenConfig_init_default(&config);
  struct LeelenDiscovery device;
  LeelenDiscovery_init(&device, &config);
  struct LeelenSIP sip;
  should (LeelenSIP_init(&sip, &device) == 0) otherwise {
    fprintf(stderr, "error: failed to initialize SIP library\n");
    LeelenDiscovery_destroy(&device);
    LeelenConfig_destroy(&config);
    return EXIT_FAILURE;
  }

  static const struct option long_options[] = {
    {"desc", required_argument, 0, 256},
    {"type", required_argument, 0, 257},
    {"audio", required_argument, 0, 258},
    {"video", required_argument, 0, 259},
    {"discovery", required_argument, 0, 260},
    {"leelen", required_argument, 0, 261},
    {"control", required_argument, 0, 262},

    {"report-addr", required_argument, 0, 264},
    {"ua", required_argument, 0, 265},

    {"interface", required_argument, 0, 'i'},
    {"port", required_argument, 0, 'p'},
    {"daemonize", no_argument, 0, 'D'},
    {"debug", no_argument, 0, 'd'},
    {"ipv6", no_argument, 0, '6'},
    {"help", no_argument, 0, 'h'},

    {0}
  };

  int n_positional = 0;
  for (int option, longindex;
       (option = getopt_long(argc, argv, "-i:p:Dd6h", long_options, &longindex))
         != -1;) {
    int offset;
    switch (option) {
      case 1:
        switch (n_positional) {
          case 0: {
            int domain = argtosockaddr(optarg, &leelen_addr);
            goto_if_fail (domain != AF_UNSPEC) fail;
            if (domain == AF_LOCAL) {
              leelen_addr.sa_family = AF_INET;
            }
            break;
          }
          case 1: {
            int err = regcomp(&device.phone, optarg, REG_EXTENDED | REG_NOSUB);
            should (err == 0) otherwise {
              char errbuf[256];
              regerror(err, &device.phone, errbuf, sizeof(errbuf));
              fprintf(stderr, "error when compiling regex '%s': %s\n",
                      optarg, errbuf);
              goto fail;
            }
            device.phone_set = true;
            break;
          }
          default:
            fprintf(stderr, "error: too many n_positional options\n");
            goto fail;
        }
        n_positional++;
        break;
      case 'i':
        should (sip_addr.sa_family == AF_UNSPEC) otherwise {
          fprintf(stderr, "error: duplicated -i option\n");
          goto fail;
        }
        goto_if_fail (argtosockaddr(optarg, &sip_addr) == AF_UNSPEC) fail;
        break;
      case 'p':
        goto_if_fail (
          argtoport(optarg, &sip_addr.sa_port, false, true, "-p", NULL) == 0
        ) fail;
        break;
      case 'D':
        daemonize = true;
        break;
      case 'd':
        logger_set_level(LOG_LEVEL_DEBUG);
        break;
      case '6':
        force_v6 = true;
        break;
      case 'h':
        usage(argv[0]);
        goto end;
      case 256:
        should (config.desc == NULL) otherwise {
          fprintf(stderr, "error: duplicated --%s option\n",
                  long_options[longindex].name);
          goto fail;
        }
        config.desc = strdup(optarg);
        should (config.desc != NULL) otherwise {
          perror("strdup");
          goto fail;
        }
        break;
      case 257: {
        int type;
        goto_if_fail (
          argtoi(optarg, &type, 1, 127, long_options[longindex].name, NULL) == 0
        ) fail;
        config.type = type;
        break;
      }
      case 258:
        offset = offsetof(struct LeelenConfig, audio);
        goto leelen_port;
      case 259:
        offset = offsetof(struct LeelenConfig, video);
        goto leelen_port;
      case 260:
        offset = offsetof(struct LeelenConfig, discovery);
        goto leelen_port;
      case 261:
        offset = offsetof(struct LeelenConfig, sip);
        goto leelen_port;
      case 262:
        offset = offsetof(struct LeelenConfig, control);
leelen_port:
        goto_if_fail (argtoport(
          optarg, (in_port_t *) ((char *) &config + offset), false, false,
          long_options[longindex].name, NULL
        ) == 0) fail;
        break;
      case 264:
        should (strlen(optarg) < sizeof(device.report_addr) - 1) otherwise {
          fprintf(
            stderr, "error: length of --%s arguement should be less than %zd\n",
            long_options[longindex].name, sizeof(device.report_addr) - 1);
          goto fail;
        }
        strcpy(device.report_addr, optarg);
        break;
      default:
        // getopt already print error for us
        // fprintf(stderr, "error: unknown option '%c'\n", option);
        goto fail;
    }
  }

  if (sip_addr.sa_family != AF_INET) {
    sip_addr.sa_family = AF_INET6;
  }
  if (leelen_addr.sa_family != AF_INET6) {
    leelen_addr.sa_family = AF_INET;
  }
  if (leelen_addr.sa_family != AF_INET6 && force_v6) {
    uint32_t scope_id = leelen_addr.sa_scope_id;
    sockaddr_to6(&leelen_addr.sock);
    leelen_addr.sa_scope_id = scope_id;
  }

  int ret =
    !!start_leelen2sip(&device, &sip, &leelen_addr, &sip_addr, daemonize);

  if (0) {
end:
    ret = EXIT_SUCCESS;
  }
  if (0) {
fail:
    ret = EXIT_FAILURE;
  }
  LeelenSIP_destroy(&sip);
  LeelenDiscovery_destroy(&device);
  LeelenConfig_destroy(&config);
  return ret;
}
