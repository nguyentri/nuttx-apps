/* SPI loopback tests — MOSI<->MISO jumper (Pin 19<->Pin 21).
 * Uses NuttX SPI char driver /dev/spi0 via SPIIOC_TRANSFER.
 * MPU9250 must be removed from header for clean loopback.
 */

#include <nuttx/config.h>
#include <nuttx/spi/spi.h>
#include <nuttx/spi/spi_transfer.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "hwtest.h"

#define SPI_DEV "/dev/spi0"

static int spi_xfer(uint32_t freq, uint8_t mode, const uint8_t *tx,
                    uint8_t *rx, size_t n)
{
  int fd = open(SPI_DEV, O_RDWR);
  if (fd < 0)
    {
      return -errno;
    }

  struct spi_trans_s     t = { 0 };
  struct spi_sequence_s  s = { 0 };

  t.deselect = true;
  t.nwords   = n;
  t.txbuffer = tx;
  t.rxbuffer = rx;

  s.dev       = SPIDEV_USER(0);
  s.mode      = mode;
  s.nbits     = 8;
  s.ntrans    = 1;
  s.frequency = freq;
  s.trans     = &t;

  int r = ioctl(fd, SPIIOC_TRANSFER, (unsigned long)&s);
  int saved = errno;
  close(fd);
  return r < 0 ? -saved : 0;
}

static int t_byte_loop_1mhz(void)
{
  uint8_t tx = 0xA5, rx = 0x00;
  int r = spi_xfer(1000000, SPIDEV_MODE0, &tx, &rx, 1);
  if (r < 0)
    {
      HWTEST_FAIL_REASON("xfer err=%d", -r);
    }
  HWTEST_ASSERT_EQ_HEX(rx, tx);
  return HWTEST_PASS;
}

static int t_byte_loop_4mhz(void)
{
  uint8_t tx = 0x5A, rx = 0x00;
  int r = spi_xfer(4000000, SPIDEV_MODE0, &tx, &rx, 1);
  if (r < 0)
    {
      HWTEST_FAIL_REASON("xfer err=%d", -r);
    }
  HWTEST_ASSERT_EQ_HEX(rx, tx);
  return HWTEST_PASS;
}

static int t_burst_64_pattern(void)
{
  uint8_t tx[64], rx[64];
  for (int i = 0; i < 64; i++) tx[i] = (uint8_t)(i ^ 0xC3);
  memset(rx, 0, sizeof(rx));

  int r = spi_xfer(1000000, SPIDEV_MODE0, tx, rx, sizeof(tx));
  if (r < 0)
    {
      HWTEST_FAIL_REASON("xfer err=%d", -r);
    }
  HWTEST_ASSERT_MEM_EQ(rx, tx, sizeof(tx));
  return HWTEST_PASS;
}

static int t_mode3(void)
{
  uint8_t tx = 0xC3, rx = 0x00;
  int r = spi_xfer(1000000, SPIDEV_MODE3, &tx, &rx, 1);
  if (r < 0)
    {
      HWTEST_FAIL_REASON("xfer err=%d", -r);
    }
  HWTEST_ASSERT_EQ_HEX(rx, tx);
  return HWTEST_PASS;
}

static const struct hwtest_case_s g_cases[] =
{
  { "spi.ch0", "byte_loop_1mhz",   t_byte_loop_1mhz   },
  { "spi.ch0", "byte_loop_4mhz",   t_byte_loop_4mhz   },
  { "spi.ch0", "burst_64_pattern", t_burst_64_pattern },
  { "spi.ch0", "mode3",            t_mode3            },
};

const struct hwtest_case_s *spi_loopback_cases(unsigned int *n)
{
  *n = sizeof(g_cases) / sizeof(g_cases[0]);
  return g_cases;
}
