/* BMP280 I2C chip-id — reg 0xD0 -> 0x58. Bus = I2C7 (P76 SDA / P77 SCL). */

#include <nuttx/config.h>
#include <nuttx/i2c/i2c_master.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "hwtest.h"

#define I2C_DEV          "/dev/i2c7"
#define BMP280_REG_ID    0xD0
#define BMP280_CHIP_ID   0x58
#define BMP280_ADDR_LOW  0x76
#define BMP280_ADDR_HIGH 0x77

static int bmp280_read_id(uint8_t addr, uint8_t *id)
{
  int fd = open(I2C_DEV, O_RDWR);
  if (fd < 0) return -errno;

  uint8_t reg = BMP280_REG_ID;
  uint8_t val = 0;

  struct i2c_msg_s msgs[2] =
  {
    { .frequency = 100000, .addr = addr, .flags = 0,
      .buffer = &reg, .length = 1 },
    { .frequency = 100000, .addr = addr, .flags = I2C_M_READ,
      .buffer = &val, .length = 1 },
  };

  struct i2c_transfer_s xfer = { .msgv = msgs, .msgc = 2 };

  int r = ioctl(fd, I2CIOC_TRANSFER, (unsigned long)&xfer);
  int saved = errno;
  close(fd);
  if (r < 0) return -saved;

  *id = val;
  return 0;
}

static int t_chipid(void)
{
  uint8_t id = 0;

  /* Try both strap addresses. */

  if (bmp280_read_id(BMP280_ADDR_LOW, &id) == 0 && id == BMP280_CHIP_ID)
    {
      return HWTEST_PASS;
    }
  if (bmp280_read_id(BMP280_ADDR_HIGH, &id) == 0 && id == BMP280_CHIP_ID)
    {
      return HWTEST_PASS;
    }

  HWTEST_FAIL_REASON("no BMP280 at 0x76/0x77 (last id=0x%02x)", id);
}

static const struct hwtest_case_s g_cases[] =
{
  { "i2c.bmp280", "chipid", t_chipid },
};

const struct hwtest_case_s *bmp280_cases(unsigned int *n)
{
  *n = sizeof(g_cases) / sizeof(g_cases[0]);
  return g_cases;
}
