/* UART loopback tests — TX<->RX jumpers per the RDK-RZ/V2H pinout.
 * Device paths follow PX4 convention (UART4 -> /dev/ttyS4, etc.); the
 * NuttX defconfig must map RZ/V2H SCI channels onto these tty numbers.
 */

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

#define UART_OPEN_TIMEOUT_MS  200
#define UART_RX_TIMEOUT_MS    250

static int uart_open_configured(const char *path, int baud)
{
  int fd = open(path, O_RDWR | O_NOCTTY);
  if (fd < 0)
    {
      return -errno;
    }

  struct termios t;
  if (tcgetattr(fd, &t) < 0)
    {
      close(fd);
      return -errno;
    }

  cfmakeraw(&t);
  cfsetispeed(&t, baud);
  cfsetospeed(&t, baud);
  t.c_cc[VMIN]  = 0;
  t.c_cc[VTIME] = 0;

  if (tcsetattr(fd, TCSANOW, &t) < 0)
    {
      close(fd);
      return -errno;
    }

  tcflush(fd, TCIOFLUSH);
  return fd;
}

static int uart_rx_blocking(int fd, void *buf, size_t want, int timeout_ms)
{
  size_t got = 0;
  uint8_t *p = (uint8_t *)buf;

  while (got < want)
    {
      struct pollfd pfd = { .fd = fd, .events = POLLIN };
      int pr = poll(&pfd, 1, timeout_ms);
      if (pr <= 0)
        {
          return -ETIMEDOUT;
        }

      ssize_t r = read(fd, p + got, want - got);
      if (r < 0)
        {
          return -errno;
        }
      got += (size_t)r;
    }

  return (int)got;
}

static int uart_loopback_byte(const char *path, int baud)
{
  int fd = uart_open_configured(path, baud);
  if (fd < 0)
    {
      HWTEST_FAIL_REASON("open(%s) failed err=%d", path, -fd);
    }

  uint8_t tx = 0xA5;
  uint8_t rx = 0x00;

  if (write(fd, &tx, 1) != 1)
    {
      close(fd);
      HWTEST_FAIL_REASON("write err=%d", errno);
    }

  int r = uart_rx_blocking(fd, &rx, 1, UART_RX_TIMEOUT_MS);
  close(fd);

  if (r < 0)
    {
      HWTEST_FAIL_REASON("rx timeout (%s @ %d)", path, baud);
    }

  HWTEST_ASSERT_EQ_HEX(rx, tx);
  return HWTEST_PASS;
}

static int uart_loopback_pattern(const char *path, int baud)
{
  int fd = uart_open_configured(path, baud);
  if (fd < 0)
    {
      HWTEST_FAIL_REASON("open(%s) failed err=%d", path, -fd);
    }

  uint8_t tx[256];
  uint8_t rx[256];
  for (int i = 0; i < 256; i++)
    {
      tx[i] = (uint8_t)i;
    }

  if (write(fd, tx, sizeof(tx)) != (ssize_t)sizeof(tx))
    {
      close(fd);
      HWTEST_FAIL_REASON("write err=%d", errno);
    }

  int r = uart_rx_blocking(fd, rx, sizeof(rx), 1000);
  close(fd);

  if (r < 0)
    {
      HWTEST_FAIL_REASON("rx incomplete err=%d", -r);
    }

  HWTEST_ASSERT_MEM_EQ(rx, tx, sizeof(tx));
  return HWTEST_PASS;
}

/* Concrete cases — one per UART x case. */

#define UART_CASES(suite, dev)                                          \
  static int t_##suite##_byte_115200(void)                              \
    { return uart_loopback_byte(dev, B115200); }                        \
  static int t_##suite##_byte_9600(void)                                \
    { return uart_loopback_byte(dev, B9600); }                          \
  static int t_##suite##_pattern_115200(void)                           \
    { return uart_loopback_pattern(dev, B115200); }

/* ttyS<N> assignment is sequential per CONFIG_RZV_SCI<X> when no SCI is
 * the system console (RTT console here). With SCI4/5/9 enabled in this
 * order: ttyS1=SCI4 (UART4), ttyS2=SCI5 (UART5), ttyS3=SCI9 (UART9).
 * See arch/arm/src/rzv/rzv_serial.c TTYSn_DEV chain.
 */
UART_CASES(uart4, "/dev/ttyS1")
UART_CASES(uart5, "/dev/ttyS2")
UART_CASES(uart9, "/dev/ttyS3")

static const struct hwtest_case_s g_cases[] =
{
  { "uart.uart4", "byte_115200",    t_uart4_byte_115200    },
  { "uart.uart4", "byte_9600",      t_uart4_byte_9600      },
  { "uart.uart4", "pattern_115200", t_uart4_pattern_115200 },
  { "uart.uart5", "byte_115200",    t_uart5_byte_115200    },
  { "uart.uart5", "byte_9600",      t_uart5_byte_9600      },
  { "uart.uart5", "pattern_115200", t_uart5_pattern_115200 },
  { "uart.uart9", "byte_115200",    t_uart9_byte_115200    },
  { "uart.uart9", "byte_9600",      t_uart9_byte_9600      },
  { "uart.uart9", "pattern_115200", t_uart9_pattern_115200 },
};

const struct hwtest_case_s *uart_loopback_cases(unsigned int *n)
{
  *n = sizeof(g_cases) / sizeof(g_cases[0]);
  return g_cases;
}
