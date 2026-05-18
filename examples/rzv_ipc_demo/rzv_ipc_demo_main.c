/****************************************************************************
 * apps/examples/rzv_ipc_demo/rzv_ipc_demo_main.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * ipctest — ping/echo exerciser for any /dev/ipccN device.
 *
 * Usage:
 *   ipctest --ping -d <dev> [-n <count>] [-s <size>] [--timeout <ms>]
 *   ipctest --echo -d <dev>
 *
 * Exit codes:
 *   0  pass
 *   1  fail (echo mismatch or I/O error)
 *   2  timeout (all sends timed out — expected when peer absent)
 *   3  usage error
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define IPCTEST_DEFAULT_DEV      "/dev/ipcc1"
#define IPCTEST_DEFAULT_COUNT    10
#define IPCTEST_DEFAULT_SIZE     32
#define IPCTEST_DEFAULT_TIMEOUT  500   /* ms — poll timeout per message */
#define IPCTEST_MAX_MSGSIZE      512

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct ipctest_args_s
{
  const char *dev;
  int         count;
  int         msgsize;
  int         timeout_ms;
  bool        ping_mode;
  bool        echo_mode;
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void usage(const char *prog)
{
  fprintf(stderr,
    "Usage:\n"
    "  %s --ping -d <dev> [-n count] [-s size] [--timeout ms]\n"
    "  %s --echo -d <dev>\n"
    "\n"
    "Modes:\n"
    "  --ping   Send N messages of S bytes, expect echo, print stats.\n"
    "  --echo   Read messages and write them back (run on peer core).\n"
    "\n"
    "Defaults: dev=%s  n=%d  s=%d  timeout=%d ms\n",
    prog, prog,
    IPCTEST_DEFAULT_DEV, IPCTEST_DEFAULT_COUNT,
    IPCTEST_DEFAULT_SIZE, IPCTEST_DEFAULT_TIMEOUT);
}

static int parse_args(int argc, char *argv[], struct ipctest_args_s *a)
{
  int i;

  /* Defaults */

  a->dev        = IPCTEST_DEFAULT_DEV;
  a->count      = IPCTEST_DEFAULT_COUNT;
  a->msgsize    = IPCTEST_DEFAULT_SIZE;
  a->timeout_ms = IPCTEST_DEFAULT_TIMEOUT;
  a->ping_mode  = false;
  a->echo_mode  = false;

  for (i = 1; i < argc; i++)
    {
      if (strcmp(argv[i], "--ping") == 0)
        {
          a->ping_mode = true;
        }
      else if (strcmp(argv[i], "--echo") == 0)
        {
          a->echo_mode = true;
        }
      else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc)
        {
          a->dev = argv[++i];
        }
      else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc)
        {
          a->count = atoi(argv[++i]);
        }
      else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc)
        {
          a->msgsize = atoi(argv[++i]);
        }
      else if (strcmp(argv[i], "--timeout") == 0 && i + 1 < argc)
        {
          a->timeout_ms = atoi(argv[++i]);
        }
      else
        {
          fprintf(stderr, "Unknown argument: %s\n", argv[i]);
          return -1;
        }
    }

  if (!a->ping_mode && !a->echo_mode)
    {
      fprintf(stderr, "Error: specify --ping or --echo\n");
      return -1;
    }

  if (a->ping_mode && a->echo_mode)
    {
      fprintf(stderr, "Error: --ping and --echo are mutually exclusive\n");
      return -1;
    }

  if (a->msgsize < 1 || a->msgsize > IPCTEST_MAX_MSGSIZE)
    {
      fprintf(stderr, "Error: -s must be 1..%d\n", IPCTEST_MAX_MSGSIZE);
      return -1;
    }

  if (a->count < 1)
    {
      fprintf(stderr, "Error: -n must be >= 1\n");
      return -1;
    }

  return 0;
}

/* Monotonic millisecond timestamp */

static uint64_t now_ms(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000u + (uint64_t)(ts.tv_nsec / 1000000u);
}

/****************************************************************************
 * Name: run_ping
 *
 * Description:
 *   Send 'count' fixed-size messages and wait for echo from peer.
 *   Uses poll() with timeout so the function never hangs indefinitely.
 *
 * Returned Value:
 *   0 = all echoes matched, 1 = mismatch/error, 2 = timeout
 *
 ****************************************************************************/

static int run_ping(int fd, const struct ipctest_args_s *a)
{
  uint8_t  txbuf[IPCTEST_MAX_MSGSIZE];
  uint8_t  rxbuf[IPCTEST_MAX_MSGSIZE];
  struct pollfd pfd;
  uint64_t t0;
  uint64_t t1;
  uint64_t lat_min = UINT64_MAX;
  uint64_t lat_max = 0;
  uint64_t lat_sum = 0;
  int      pass    = 0;
  int      timeouts = 0;
  int      i;
  int      ret;
  ssize_t  nw;
  ssize_t  nr;

  pfd.fd     = fd;
  pfd.events = POLLIN;

  for (i = 0; i < a->count; i++)
    {
      /* Build payload: first 4 bytes = message index (LE), rest = 0xAA */

      memset(txbuf, 0xaa, (size_t)a->msgsize);
      txbuf[0] = (uint8_t)(i & 0xff);
      txbuf[1] = (uint8_t)((i >> 8) & 0xff);
      txbuf[2] = (uint8_t)((i >> 16) & 0xff);
      txbuf[3] = (uint8_t)((i >> 24) & 0xff);

      t0 = now_ms();

      nw = write(fd, txbuf, (size_t)a->msgsize);
      if (nw < 0)
        {
          if (errno == ETIMEDOUT)
            {
              fprintf(stderr, "[%d] write timeout (no peer?)\n", i);
              timeouts++;
              continue;
            }

          fprintf(stderr, "[%d] write error: %d\n", i, errno);
          return 1;
        }

      /* Wait for echo with poll timeout */

      ret = poll(&pfd, 1, a->timeout_ms);
      if (ret == 0)
        {
          fprintf(stderr, "[%d] poll timeout waiting for echo\n", i);
          timeouts++;
          continue;
        }

      if (ret < 0)
        {
          fprintf(stderr, "[%d] poll error: %d\n", i, errno);
          return 1;
        }

      nr = read(fd, rxbuf, (size_t)a->msgsize);
      if (nr < 0)
        {
          fprintf(stderr, "[%d] read error: %d\n", i, errno);
          return 1;
        }

      t1 = now_ms();

      /* Verify echo */

      if ((size_t)nr != (size_t)a->msgsize ||
          memcmp(txbuf, rxbuf, (size_t)a->msgsize) != 0)
        {
          fprintf(stderr, "[%d] echo mismatch (sent %d, got %d)\n",
                  i, a->msgsize, (int)nr);
          return 1;
        }

      pass++;
      uint64_t lat = t1 - t0;
      if (lat < lat_min) lat_min = lat;
      if (lat > lat_max) lat_max = lat;
      lat_sum += lat;
    }

  /* Summary */

  printf("ipctest ping: %d/%d pass, %d timeout\n",
         pass, a->count, timeouts);

  if (pass > 0)
    {
      printf("  latency min/avg/max = %llu/%llu/%llu ms\n",
             (unsigned long long)lat_min,
             (unsigned long long)(lat_sum / (uint64_t)pass),
             (unsigned long long)lat_max);
    }

  if (timeouts == a->count)
    {
      return 2;   /* all timed out — peer absent */
    }

  return (pass == a->count) ? 0 : 1;
}

/****************************************************************************
 * Name: run_echo
 *
 * Description:
 *   Read messages and write them back until EOF or SIGINT.
 *   Uses O_NONBLOCK + poll so it doesn't hang when peer is gone.
 *
 * Returned Value:
 *   Always 0 (echo loops exit cleanly on EOF).
 *
 ****************************************************************************/

static int run_echo(int fd, const struct ipctest_args_s *a)
{
  uint8_t       buf[IPCTEST_MAX_MSGSIZE];
  struct pollfd pfd;
  ssize_t       nr;
  ssize_t       nw;
  int           ret;
  int           echoed = 0;

  pfd.fd     = fd;
  pfd.events = POLLIN;

  printf("ipctest echo: listening on %s (Ctrl-C to stop)\n", a->dev);

  for (;;)
    {
      ret = poll(&pfd, 1, a->timeout_ms);
      if (ret == 0)
        {
          continue;   /* timeout, keep waiting */
        }

      if (ret < 0)
        {
          if (errno == EINTR)
            {
              break;
            }

          fprintf(stderr, "echo poll error: %d\n", errno);
          break;
        }

      nr = read(fd, buf, sizeof(buf));
      if (nr <= 0)
        {
          break;   /* EOF or error */
        }

      nw = write(fd, buf, (size_t)nr);
      if (nw < 0)
        {
          fprintf(stderr, "echo write error: %d\n", errno);
          break;
        }

      echoed++;
    }

  printf("ipctest echo: %d messages echoed\n", echoed);
  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  struct ipctest_args_s args;
  int fd;
  int ret;

  if (parse_args(argc, argv, &args) < 0)
    {
      usage(argv[0]);
      return 3;
    }

  fd = open(args.dev, O_RDWR | O_NONBLOCK);
  if (fd < 0)
    {
      fprintf(stderr, "ipctest: cannot open %s: %d\n", args.dev, errno);
      return 1;
    }

  if (args.ping_mode)
    {
      ret = run_ping(fd, &args);
    }
  else
    {
      ret = run_echo(fd, &args);
    }

  close(fd);
  return ret;
}
