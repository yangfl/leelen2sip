#include <errno.h>
#include <getopt.h>
#include <regex.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <inet46i/endian.h>
#include <inet46i/in46.h>
#include <inet46i/sockaddr46.h>

#include "utils/macro.h"
#include "utils/arg.h"
#include "utils/log.h"
#include "utils/single.h"
#include "leelen/config.h"
#include "leelen/family.h"
#include "leelen/number.h"
#include "leelen/discovery/discovery.h"
#include "leelen/discovery/protocol.h"
#include "sipleelen/sipleelen.h"
#include "leelen2sip.h"


struct SIPLeelen *sipleelen = NULL;


static void shutdown_leelen2sip (int sig) {
  (void) sig;

  if likely (single_is_running(&sipleelen->state)) {
    LOG(LOG_LEVEL_INFO, "Shutting down " LEELEN2SIP_NAME);
    single_stop(&sipleelen->state);
  }
}


/**
 * @ingroup leelen2sip
 * @brief LEELEN2SIP main function.
 *
 * @param sip SIP user agent.
 * @param daemonize @c true if run as daemon.
 * @return 0 on success, error otherwise.
 */
static int start_leelen2sip (
    struct SIPLeelen *sip, bool daemonize) {
  union sockaddr_in46 *sip_addr = &sip->addr;
  struct LeelenDiscovery *device = &sip->device;
  const struct LeelenConfig *config = device->config;

  // check ports
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < i; j++) {
      should (config->ports[i] != config->ports[j]) otherwise {
        fprintf(stderr, "error: LEELEN ports conflicted\n");
        return 255;
      }
    }
  }
  in_port_t sip_port = ntohs(sip_addr->sa_port);
  if (config->discovery == sip_port || config->control == sip_port) {
    // same ip -> different interfaces
    struct in64_addr leelen_host;
    struct in64_addr sip_host;
    in64_set(config->addr.sa_family, &leelen_host,
             sockaddr_addr(&config->addr.sock));
    in64_set(sip_addr->sa_family, &sip_host, sockaddr_addr(&sip_addr->sock));
    if ((IN64_IS_ADDR_UNSPECIFIED(&leelen_host) &&
         IN64_IS_ADDR_UNSPECIFIED(&sip_host)) ||
        memcmp(&leelen_host, &sip_host, sizeof(struct in6_addr)) == 0) {
      should (sip_addr->sa_scope_id != config->addr.sa_scope_id) otherwise {
        fprintf(stderr, "error: LEELEN and SIP listen to the same port on the "
                        "same address/interface!\n");
        return 255;
      }
    }
  }

  // open sockets
  int res = SIPLeelen_connect(sip);
  should (res == 0) otherwise {
    int port;
    const char *name;
    switch (res) {
      case 1:
        port = config->discovery;
        name = LEELEN_DISCOVERY_PROTOCOL_NAME;
        break;
      case 2:
        port = config->voip;
        name = LEELEN_VOIP_PROTOCOL_NAME;
        break;
      case 3:
        port = config->control;
        name = LEELEN_CONRTOL_PROTOCOL_NAME;
        break;
      case 4:
        port = ntohs(sip_addr->sa_port);
        name = "SIP";
        break;
      default:
        port = res;
        name = "(Unknown)";
        break;
    }
    fprintf(stderr, "cannot open port %d for protocol %s: ", port, name);
    perror(NULL);
    return -1;
  }

  // set up LEELEN discovery
  if likely (device->report_addr[0] == '\0') {
    LeelenAdvertiser_fix_report_addr((struct LeelenAdvertiser *) device);
  }
  LOG(LOG_LEVEL_INFO, "Phone number: %s", config->number.str);
  LOG(LOG_LEVEL_INFO, "Device desc: " LEELEN_DISCOVERY_FORMAT,
      device->report_addr[0] == '\0' ? "<unspecified>" : device->report_addr,
      config->type, config->desc);

  // daemonize
  if (daemonize) {
    should (daemon(0, 0) == 0) otherwise {
      perror("daemon");
      return -1;
    }
  }

  // main loop
  srand(time(NULL));
  LOG(LOG_LEVEL_NOTICE, "Start " LEELEN2SIP_NAME);
  sipleelen = sip;
  signal(SIGINT, shutdown_leelen2sip);
  should (SIPLeelen_run(sip) == 0) otherwise {
    LOG(LOG_LEVEL_ERROR, "Cannot start SIP thread");
    return -1;
  }

  // end
  if unlikely (single_is_running(&sip->state)) {
    single_stop(&sip->state);
  }
  return 0;
}


static void usage (const char progname[]) {
  fprintf(stdout, "Usage: %s [OPTIONS...] <number> [<interface>]\n",
          progname);
  fprintf(stdout,
"Convert LEELEN Video Intercom System to standard SIP protocol\n"
"\n"
"  <interface>        interface or address for LEELEN protocols\n"
"  <number>           this phone number (usually XXXX-XXXX[-X])\n"
"  -i, --interface <interface>\n"
"                     interface or address for SIP protocol (default: [::])\n"
"  -p, --port <port>  SIP port (default: " STR(SIPLEELEN_PORT) ")\n"
"  -D, --daemonize    daemonize (run in background)\n"
"  -d, --debug        enable debugging messages\n"
"  -6, --ipv6         use IPv6 for LEELEN protocols (very dangerous)\n"
"  -h, --help         print this help text\n"
"\n");
  fprintf(stdout,
"Advanced options:\n"
"  --report-addr <address>\n"
"                      override address for reporting (default: extract from\n"
"                      LEELEN listening interface)\n"
"                      WARNING: this option does not check for IP validness\n"
"  --ua <ua>           SIP user agent (default: " LEELEN2SIP_USER_AGENT ")\n"
"  --reply-to <regex>  reply to LEELEN discovery when phone number match <regex>\n"
"\n");
  fprintf(stdout,
"LEELEN SIP options:\n"
"WARNING: Change these options may cause incompatibility!\n"
"  --desc <desc>              device description (default:\n"
"                             " LEELEN2SIP_DEVICE_DESC ")\n"
"  --type <int>               device type (default: %d)\n"
"  --audio <port>             audio port (default: " STR(LEELEN_AUDIO_PORT) ")\n"
"  --video <port>             video port (default: " STR(LEELEN_VIDEO_PORT) ")\n"
"  --discovery <port>         discovery port (default: " STR(LEELEN_DISCOVERY_PORT) ")\n"
"  --voip <port>              VoIP port (default: " STR(LEELEN_VOIP_PORT) ")\n"
"  --control <port>           control port (default: " STR(LEELEN_CONRTOL_PORT) ")\n"
"  --discovery-listen <port>  discovery port (default: " STR(LEELEN_DISCOVERY_PORT) ")\n"
"  --voip-listen <port>       VoIP port (default: " STR(LEELEN_VOIP_PORT) ")\n"
"  --control-listen <port>    control port (default: " STR(LEELEN_CONRTOL_PORT) ")\n",
    LEELEN_DEVICE_BASIC);
}


/**
 * @ingroup leelen2sip
 * @brief Main entrcpoint.
 *
 * @param argc Number of arguments.
 * @param argv Argument list.
 * @return `EXIT_SUCCESS` on success, `EXIT_FAILURE` on error.
 */
int main (int argc, char *argv[]) {
  should (argc > 1) otherwise {
    usage(argv[0]);
    return EXIT_FAILURE;
  }

  diagnose_sigsegv(false, 0, NULL);

  // config variables
  bool daemonize = false;
  bool use_v6 = false;
  struct LeelenConfig config;
  LeelenConfig_init(&config);
  struct SIPLeelen sip;
  should (SIPLeelen_init(&sip, &config) == 0) otherwise {
    perror("error: failed to initialize SIP library");
    LeelenConfig_destroy(&config);
    return EXIT_FAILURE;
  }
  union sockaddr_in46 *sip_addr = &sip.addr;
  sip_addr->sa_family = AF_UNSPEC;
  struct LeelenDiscovery *device = &sip.device;

  // parse options
  static const struct option long_options[] = {
    {"report-addr", required_argument, 0, 256},
    {"ua", required_argument, 0, 257},
    {"reply-to", required_argument, 0, 258},

    {"desc", required_argument, 0, 512},
    {"type", required_argument, 0, 513},
    {"audio", required_argument, 0, 514},
    {"video", required_argument, 0, 515},
    {"discovery", required_argument, 0, 516},
    {"voip", required_argument, 0, 517},
    {"control", required_argument, 0, 518},
    {"discovery-listen", required_argument, 0, 519},
    {"voip-listen", required_argument, 0, 520},
    {"control-listen", required_argument, 0, 521},

    {"interface", required_argument, 0, 'i'},
    {"port", required_argument, 0, 'p'},
    {"daemonize", no_argument, 0, 'D'},
    {"debug", no_argument, 0, 'd'},
    {"ipv6", no_argument, 0, '6'},
    {"help", no_argument, 0, 'h'},

    {0}
  };

  int n_positional = 0;
  while (1) {
    int longindex;
    int index = getopt_long(argc, argv, "-i:p:Dd6h", long_options, &longindex);
    break_if_fail (index >= 0);

    switch (index) {
      case 1:
        switch (n_positional) {
          case 0:
            switch (LeelenNumber_init(&config.number, optarg, NULL)) {
              case 0:
                break;
              default:
                fprintf(stderr, "error: invalid phone number format\n");
                goto fail;
              case 2:
                fprintf(stderr, "error: missing block number\n");
                goto fail;
            }
            break;
          case 1:
            config.addr.sa_family = argtosockaddr(optarg, &config.addr);
            goto_if_fail (config.addr.sa_family != AF_UNSPEC) fail;
            break;
          default:
            fprintf(stderr, "error: too many positional options\n");
            goto fail;
        }
        n_positional++;
        break;
      case 'i':
        should (sip_addr->sa_family == AF_UNSPEC) otherwise {
          fprintf(stderr, "error: duplicated -i option\n");
          goto fail;
        }
        goto_if_fail (argtosockaddr(optarg, sip_addr) == AF_UNSPEC) fail;
        break;
      case 'p':
        goto_if_fail (
          argtoport(optarg, &sip_addr->sa_port, false, true, "-p", NULL) == 0
        ) fail;
        break;
      case 'D':
        daemonize = true;
        break;
      case 'd':
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-write-to-const"
        LOGGER_SET_ATTRIBUTE(level, max(LOG_LEVEL_DEBUG, app_logger.level + 1));
#pragma GCC diagnostic pop
        break;
      case '6':
        use_v6 = true;
        break;
      case 'h':
        usage(argv[0]);
        goto end;
      case 256:
        should (strlen(optarg) < sizeof(device->report_addr) - 1) otherwise {
          fprintf(
            stderr, "error: length of --%s argument should be less than %zd\n",
            long_options[longindex].name, sizeof(device->report_addr) - 1);
          goto fail;
        }
        strcpy(device->report_addr, optarg);
        break;
      case 257:
        free(sip.ua);
        sip.ua = strdup(optarg);
        should (sip.ua != NULL) otherwise {
          perror("strdup");
          goto fail;
        }
        break;
      case 258: {
        int err =
          regcomp(&device->number_regex, optarg, REG_EXTENDED | REG_NOSUB);
        should (err == 0) otherwise {
          char errbuf[256];
          regerror(err, &device->number_regex, errbuf, sizeof(errbuf));
          fprintf(stderr, "error when compiling regex '%s': %s\n",
                  optarg, errbuf);
          goto fail;
        }
        device->number_regex_set = true;
        break;
      }
      case 512:
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
      case 513: {
        int type;
        goto_if_fail (
          argtoi(optarg, &type, 1, 127, long_options[longindex].name, NULL) == 0
        ) fail;
        config.type = type;
        break;
      }
      default:
        if (514 <= index && index <= 521) {
          in_port_t port;
          goto_if_fail (argtoport(
            optarg, &port, false, false, long_options[longindex].name, NULL
          ) == 0) fail;
          switch (index) {
            case 514:
              config.audio = port;
              break;
            case 515:
              config.video = port;
              break;
            case 516:
              config.discovery = port;
              // fall through
            case 519:
              config.discovery_src = port;
              break;
            case 517:
              config.voip = port;
              // fall through
            case 520:
              config.voip_src = port;
              break;
            case 518:
              config.control = port;
              // fall through
            case 521:
              config.control_src = port;
              break;
          }
        } else {
          // getopt already print error for us
          // fprintf(stderr, "error: unknown option '%c'\n", option);
          goto fail;
        }
    }
  }

  should (config.number.str[0] != '\0') otherwise {
    fprintf(stderr, "error: no phone number set\n");
    goto fail;
  }
  if likely (config.desc == NULL) {
    config.desc = strdup(LEELEN2SIP_DEVICE_DESC);
    should (config.desc != NULL) otherwise {
      perror("strdup");
      goto fail;
    }
  }
  if likely (sip.ua == NULL) {
    sip.ua = strdup(LEELEN2SIP_USER_AGENT);
    should (sip.ua != NULL) otherwise {
      perror("strdup");
      goto fail;
    }
  }
  if (sip_addr->sa_family != AF_INET) {
    sip_addr->sa_family = AF_INET6;
  }
  // handle interface spec
  if (config.addr.sa_family == AF_LOCAL) {
    config.addr.sa_family = use_v6 ? AF_INET6 : AF_INET;
  }

  int ret = start_leelen2sip(&sip, daemonize) == 0 ?
    EXIT_SUCCESS : EXIT_FAILURE;

  if (0) {
end:
    ret = EXIT_SUCCESS;
  }
  if (0) {
fail:
    ret = EXIT_FAILURE;
  }
  SIPLeelen_destroy(&sip);
  LeelenConfig_destroy(&config);
  return ret;
}
