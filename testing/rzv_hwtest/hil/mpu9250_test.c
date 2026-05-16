/* MPU9250 SPI WHO_AM_I — reg 0x75 -> 0x71. */

#include <nuttx/config.h>
#include <nuttx/spi/spi.h>
#include <nuttx/spi/spi_transfer.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "hwtest.h"

#define SPI_DEV "/dev/spi0"
#define MPU9250_WHOAMI_REG  0x75
#define MPU9250_WHOAMI_VAL  0x71
#define MPU9250_READ_BIT    0x80

static int mpu9250_read_reg(uint8_t reg, uint8_t *val)
{
  int fd = open(SPI_DEV, O_RDWR);
  if (fd < 0) return -errno;

  uint8_t tx[2] = { (uint8_t)(reg | MPU9250_READ_BIT), 0x00 };
  uint8_t rx[2] = { 0 };

  struct spi_trans_s    t = { 0 };
  struct spi_sequence_s s = { 0 };
  t.deselect = true;
  t.nwords   = 2;
  t.txbuffer = tx;
  t.rxbuffer = rx;
  s.dev       = SPIDEV_USER(0);
  s.mode      = SPIDEV_MODE0;
  s.nbits     = 8;
  s.ntrans    = 1;
  s.frequency = 1000000;
  s.trans     = &t;

  int r = ioctl(fd, SPIIOC_TRANSFER, (unsigned long)&s);
  int saved = errno;
  close(fd);
  if (r < 0) return -saved;

  *val = rx[1];
  return 0;
}

static int t_whoami(void)
{
  uint8_t v = 0;
  int r = mpu9250_read_reg(MPU9250_WHOAMI_REG, &v);
  if (r < 0) HWTEST_FAIL_REASON("spi err=%d", -r);
  HWTEST_ASSERT_EQ_HEX(v, MPU9250_WHOAMI_VAL);
  return HWTEST_PASS;
}

static const struct hwtest_case_s g_cases[] =
{
  { "spi.mpu9250", "whoami", t_whoami },
};

const struct hwtest_case_s *mpu9250_cases(unsigned int *n)
{
  *n = sizeof(g_cases) / sizeof(g_cases[0]);
  return g_cases;
}
