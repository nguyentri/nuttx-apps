/* GPS M10 NMEA sniff — within 5s, see at least 1 valid $GP*<csum>\n frame. */

#include <nuttx/config.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <poll.h>

#include "hwtest.h"

/* SCI9 (UART9) → ttyS3 in hil-full defconfig; ttyS<N> is sequential per
 * enabled CONFIG_RZV_SCI<X> when no SCI is the system console (RTT here).
 * SCI4=ttyS1, SCI5=ttyS2, SCI9=ttyS3.
 */
#define GPS_DEV "/dev/ttyS3"
#define GPS_BAUD B9600
#define GPS_WINDOW_MS 5000

static int parse_one_nmea(const char *line)
{
  /* Expect "$GP..*HH\r\n" or "$GN..*HH\r\n". */

  if (line[0] != '$') return 0;
  if (line[1] != 'G') return 0;
  const char *star = strchr(line, '*');
  if (!star || star - line < 5) return 0;

  uint8_t cs = 0;
  for (const char *p = line + 1; p < star; p++) cs ^= (uint8_t)*p;

  unsigned exp = 0;
  if (sscanf(star + 1, "%02x", &exp) != 1) return 0;
  return cs == (uint8_t)exp;
}

static int t_nmea_frame(void)
{
  int fd = open(GPS_DEV, O_RDONLY | O_NOCTTY);
  if (fd < 0) HWTEST_FAIL_REASON("open(%s) err=%d", GPS_DEV, errno);

  struct termios t;
  tcgetattr(fd, &t);
  cfmakeraw(&t);
  cfsetispeed(&t, GPS_BAUD);
  cfsetospeed(&t, GPS_BAUD);
  tcsetattr(fd, TCSANOW, &t);
  tcflush(fd, TCIFLUSH);

  char buf[256];
  size_t off = 0;
  int remaining = GPS_WINDOW_MS;

  while (remaining > 0)
    {
      struct pollfd pfd = { .fd = fd, .events = POLLIN };
      int pr = poll(&pfd, 1, 500);
      remaining -= 500;
      if (pr <= 0) continue;

      ssize_t r = read(fd, buf + off, sizeof(buf) - 1 - off);
      if (r <= 0) continue;
      off += (size_t)r;
      buf[off] = '\0';

      char *nl;
      while ((nl = strchr(buf, '\n')) != NULL)
        {
          *nl = '\0';
          char *cr = strchr(buf, '\r');
          if (cr) *cr = '\0';

          if (parse_one_nmea(buf))
            {
              close(fd);
              return HWTEST_PASS;
            }

          size_t consumed = (nl - buf) + 1;
          memmove(buf, nl + 1, off - consumed);
          off -= consumed;
          buf[off] = '\0';
        }

      if (off >= sizeof(buf) - 1)
        {
          /* No newline found in full buffer — drop. */

          off = 0;
        }
    }

  close(fd);
  HWTEST_FAIL_REASON("no valid NMEA in %dms", GPS_WINDOW_MS);
}

static const struct hwtest_case_s g_cases[] =
{
  { "uart.gps", "nmea_frame", t_nmea_frame },
};

const struct hwtest_case_s *gps_cases(unsigned int *n)
{
  *n = sizeof(g_cases) / sizeof(g_cases[0]);
  return g_cases;
}
