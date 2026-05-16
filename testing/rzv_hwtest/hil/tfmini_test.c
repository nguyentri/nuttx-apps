/* TFmini Plus frame sniff — within 2s, see 0x59 0x59 header + valid checksum. */

#include <nuttx/config.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <poll.h>

#include "hwtest.h"

/* SCI4 (UART4) → ttyS1 in hil-full defconfig; see gps_nmea_test.c note */
#define TFMINI_DEV "/dev/ttyS1"
#define TFMINI_BAUD B115200
#define TFMINI_WINDOW_MS 2000
#define TFMINI_FRAME_LEN 9

static int tfmini_valid_frame(const uint8_t *f)
{
  if (f[0] != 0x59 || f[1] != 0x59) return 0;
  uint8_t cs = 0;
  for (int i = 0; i < TFMINI_FRAME_LEN - 1; i++) cs += f[i];
  return cs == f[TFMINI_FRAME_LEN - 1];
}

static int t_tfmini_frame(void)
{
  int fd = open(TFMINI_DEV, O_RDONLY | O_NOCTTY);
  if (fd < 0) HWTEST_FAIL_REASON("open(%s) err=%d", TFMINI_DEV, errno);

  struct termios t;
  tcgetattr(fd, &t);
  cfmakeraw(&t);
  cfsetispeed(&t, TFMINI_BAUD);
  cfsetospeed(&t, TFMINI_BAUD);
  tcsetattr(fd, TCSANOW, &t);
  tcflush(fd, TCIFLUSH);

  uint8_t buf[64];
  size_t off = 0;
  int remaining = TFMINI_WINDOW_MS;

  while (remaining > 0)
    {
      struct pollfd pfd = { .fd = fd, .events = POLLIN };
      int pr = poll(&pfd, 1, 200);
      remaining -= 200;
      if (pr <= 0) continue;

      ssize_t r = read(fd, buf + off, sizeof(buf) - off);
      if (r <= 0) continue;
      off += (size_t)r;

      /* Scan for valid frame. */

      for (size_t i = 0; i + TFMINI_FRAME_LEN <= off; i++)
        {
          if (tfmini_valid_frame(buf + i))
            {
              close(fd);
              return HWTEST_PASS;
            }
        }

      /* Compact: keep last TFMINI_FRAME_LEN-1 bytes for cross-boundary frame. */

      if (off >= sizeof(buf) - TFMINI_FRAME_LEN)
        {
          memmove(buf, buf + off - (TFMINI_FRAME_LEN - 1),
                  TFMINI_FRAME_LEN - 1);
          off = TFMINI_FRAME_LEN - 1;
        }
    }

  close(fd);
  HWTEST_FAIL_REASON("no valid TFmini frame in %dms", TFMINI_WINDOW_MS);
}

static const struct hwtest_case_s g_cases[] =
{
  { "uart.tfmini", "frame", t_tfmini_frame },
};

const struct hwtest_case_s *tfmini_cases(unsigned int *n)
{
  *n = sizeof(g_cases) / sizeof(g_cases[0]);
  return g_cases;
}
