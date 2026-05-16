/* HIL full aggregator — concatenates per-sensor case lists into one. */

#include <nuttx/config.h>
#include <string.h>

#include "hwtest.h"

#define HWTEST_HIL_MAX_CASES 32

static struct hwtest_case_s g_combined[HWTEST_HIL_MAX_CASES];

const struct hwtest_case_s *hil_full_cases(unsigned int *n)
{
  unsigned int total = 0;
  unsigned int k;
  const struct hwtest_case_s *src;

  src = mpu9250_cases(&k);
  if (total + k > HWTEST_HIL_MAX_CASES) k = HWTEST_HIL_MAX_CASES - total;
  memcpy(&g_combined[total], src, k * sizeof(struct hwtest_case_s));
  total += k;

  src = bmp280_cases(&k);
  if (total + k > HWTEST_HIL_MAX_CASES) k = HWTEST_HIL_MAX_CASES - total;
  memcpy(&g_combined[total], src, k * sizeof(struct hwtest_case_s));
  total += k;

  src = gps_cases(&k);
  if (total + k > HWTEST_HIL_MAX_CASES) k = HWTEST_HIL_MAX_CASES - total;
  memcpy(&g_combined[total], src, k * sizeof(struct hwtest_case_s));
  total += k;

  src = tfmini_cases(&k);
  if (total + k > HWTEST_HIL_MAX_CASES) k = HWTEST_HIL_MAX_CASES - total;
  memcpy(&g_combined[total], src, k * sizeof(struct hwtest_case_s));
  total += k;

  *n = total;
  return g_combined;
}
