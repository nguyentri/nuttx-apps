/* Hardware test main — boots, emits banner, runs mode-specific cases. */

#include <nuttx/config.h>
#include <stdio.h>
#include <unistd.h>

#include "hwtest.h"

int main(int argc, char *argv[])
{
  const struct hwtest_case_s *cases = NULL;
  unsigned int n = 0;
  const char *mode = "unknown";
  const char *wiring = "unknown";

  /* Let serial/RTT settle. */

  usleep(200 * 1000);

#if defined(CONFIG_TESTING_RZV_HWTEST_MODE_UART_LOOPBACK)
  mode = "uart-loopback";
  wiring = "uart4-tx-rx,uart5-tx-rx,uart9-tx-rx";
  cases = uart_loopback_cases(&n);
#elif defined(CONFIG_TESTING_RZV_HWTEST_MODE_SPI_LOOPBACK)
  mode = "spi-loopback";
  wiring = "spi0-mosi-miso,mpu9250-absent";
  cases = spi_loopback_cases(&n);
#elif defined(CONFIG_TESTING_RZV_HWTEST_MODE_HIL_FULL)
  mode = "hil-full";
  wiring = "mpu9250+bmp280+gps-m10+tfmini-per-rdk-rzv2h-pinout";
  cases = hil_full_cases(&n);
#endif

  hwtest_emit_banner(mode, wiring);

  if (cases == NULL || n == 0)
    {
      syslog(LOG_ERR, "TEST_SUMMARY: passed=0 failed=0 skipped=0\n");
      syslog(LOG_ERR, "TEST_DONE: exit=1\n");
      return 1;
    }

  return hwtest_run(cases, n);
}
