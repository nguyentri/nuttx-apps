/* RZ/V2H hardware test harness — RTT-based PASS/FAIL grammar.
 * Grammar consumed by scripts/ci/parse-results.py:
 *   TEST_ENV: mode=<m> expects=<wiring>
 *   TEST: <suite>.<name> START
 *   TEST: <suite>.<name> PASS [k=v ...]
 *   TEST: <suite>.<name> FAIL reason="..."
 *   TEST: <suite>.<name> SKIP reason="..."
 *   TEST_SUMMARY: passed=N failed=N skipped=N
 *   TEST_DONE: exit=<0|1>
 */

#ifndef __APPS_TESTING_RZV_HWTEST_HWTEST_H
#define __APPS_TESTING_RZV_HWTEST_HWTEST_H

#include <stdint.h>
#include <syslog.h>

enum hwtest_result_e
{
  HWTEST_PASS = 0,
  HWTEST_FAIL = 1,
  HWTEST_SKIP = 2,
};

typedef int (*hwtest_fn_t)(void);

struct hwtest_case_s
{
  const char *suite;
  const char *name;
  hwtest_fn_t fn;
};

void hwtest_emit_banner(const char *mode, const char *wiring);
int  hwtest_run(const struct hwtest_case_s *cases, unsigned int n);

/* Per-test reason buffer (set by TEST_ASSERT* macros on failure). */

extern char g_hwtest_fail_reason[128];

/* Assertion macros — return non-zero on failure with reason recorded. */

#define HWTEST_FAIL_REASON(fmt, ...) \
  do { snprintf(g_hwtest_fail_reason, sizeof(g_hwtest_fail_reason), \
                fmt, ##__VA_ARGS__); return HWTEST_FAIL; } while (0)

#define HWTEST_ASSERT_TRUE(cond) \
  do { if (!(cond)) HWTEST_FAIL_REASON("assert_true(%s)", #cond); } while (0)

#define HWTEST_ASSERT_EQ_INT(actual, expected) \
  do { long _a = (long)(actual), _e = (long)(expected); \
       if (_a != _e) HWTEST_FAIL_REASON("expected=%ld actual=%ld", _e, _a); \
  } while (0)

#define HWTEST_ASSERT_EQ_HEX(actual, expected) \
  do { unsigned long _a = (unsigned long)(actual), \
                    _e = (unsigned long)(expected); \
       if (_a != _e) HWTEST_FAIL_REASON("expected=0x%lx actual=0x%lx", _e, _a); \
  } while (0)

#define HWTEST_ASSERT_MEM_EQ(actual, expected, len) \
  do { if (memcmp((actual), (expected), (len)) != 0) \
         HWTEST_FAIL_REASON("memcmp mismatch len=%d", (int)(len)); \
  } while (0)

/* Per-mode test list providers. */

#ifdef CONFIG_TESTING_RZV_HWTEST_MODE_UART_LOOPBACK
const struct hwtest_case_s *uart_loopback_cases(unsigned int *n);
#endif

#ifdef CONFIG_TESTING_RZV_HWTEST_MODE_SPI_LOOPBACK
const struct hwtest_case_s *spi_loopback_cases(unsigned int *n);
#endif

#ifdef CONFIG_TESTING_RZV_HWTEST_MODE_HIL_FULL
const struct hwtest_case_s *hil_full_cases(unsigned int *n);
const struct hwtest_case_s *mpu9250_cases(unsigned int *n);
const struct hwtest_case_s *bmp280_cases(unsigned int *n);
const struct hwtest_case_s *gps_cases(unsigned int *n);
const struct hwtest_case_s *tfmini_cases(unsigned int *n);
#endif

#endif /* __APPS_TESTING_RZV_HWTEST_HWTEST_H */
