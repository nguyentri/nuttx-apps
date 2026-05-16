/* Hardware test harness — runs registered cases, emits RTT grammar. */

#include <nuttx/config.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>

#include "hwtest.h"

char g_hwtest_fail_reason[128];

void hwtest_emit_banner(const char *mode, const char *wiring)
{
  syslog(LOG_INFO, "TEST_ENV: mode=%s expects=%s\n", mode, wiring);
}

int hwtest_run(const struct hwtest_case_s *cases, unsigned int n)
{
  unsigned int passed = 0, failed = 0, skipped = 0;

  for (unsigned int i = 0; i < n; i++)
    {
      const struct hwtest_case_s *c = &cases[i];
      g_hwtest_fail_reason[0] = '\0';

      syslog(LOG_INFO, "TEST: %s.%s START\n", c->suite, c->name);

      int r = c->fn();

      if (r == HWTEST_PASS)
        {
          syslog(LOG_INFO, "TEST: %s.%s PASS\n", c->suite, c->name);
          passed++;
        }
      else if (r == HWTEST_SKIP)
        {
          syslog(LOG_INFO, "TEST: %s.%s SKIP reason=\"%s\"\n",
                 c->suite, c->name,
                 g_hwtest_fail_reason[0] ? g_hwtest_fail_reason : "unspecified");
          skipped++;
        }
      else
        {
          syslog(LOG_INFO, "TEST: %s.%s FAIL reason=\"%s\"\n",
                 c->suite, c->name,
                 g_hwtest_fail_reason[0] ? g_hwtest_fail_reason : "unspecified");
          failed++;
        }
    }

  syslog(LOG_INFO, "TEST_SUMMARY: passed=%u failed=%u skipped=%u\n",
         passed, failed, skipped);
  syslog(LOG_INFO, "TEST_DONE: exit=%d\n", failed == 0 ? 0 : 1);

  return failed == 0 ? 0 : 1;
}
