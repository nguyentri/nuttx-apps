/****************************************************************************
 * apps/examples/rzv_dmac/rzv_dmac_main.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arch/arm/src/rzv/rzv_dmac.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define RZV_DMAC_TEST_CHANNEL       RZV_DMAC_CHANNEL_0
#define RZV_DMAC_TEST_BUFFER_SIZE   1024u
#define RZV_DMAC_TEST_POLLS         1000000u

/****************************************************************************
 * Private Data
 ****************************************************************************/

static uint8_t g_src_buffer[RZV_DMAC_TEST_BUFFER_SIZE]
  __attribute__((aligned(128)));
static uint8_t g_dst_buffer[RZV_DMAC_TEST_BUFFER_SIZE]
  __attribute__((aligned(128)));

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int rzv_dmac_memcpy_test(void)
{
  struct rzv_dmac_config_s config;
  uint32_t status = 0;
  uint32_t index;
  int ret;

  for (index = 0; index < RZV_DMAC_TEST_BUFFER_SIZE; index++)
    {
      g_src_buffer[index] = (uint8_t)index;
    }

  memset(g_dst_buffer, 0, sizeof(g_dst_buffer));
  memset(&config, 0, sizeof(config));

  config.src_size = RZV_DMAC_SIZE_4BYTE;
  config.dst_size = RZV_DMAC_SIZE_4BYTE;
  config.src_addr_mode = RZV_DMAC_ADDR_INCREMENT;
  config.dst_addr_mode = RZV_DMAC_ADDR_INCREMENT;
  config.trigger = RZV_DMAC_TRIGGER_SW;
  config.src_addr = (uintptr_t)g_src_buffer;
  config.dst_addr = (uintptr_t)g_dst_buffer;
  config.length = RZV_DMAC_TEST_BUFFER_SIZE;
  config.elc_event = -1;

  printf("DMAC memcpy: cpu src=%p dst=%p bytes=%u\n",
         (FAR void *)g_src_buffer, (FAR void *)g_dst_buffer,
         RZV_DMAC_TEST_BUFFER_SIZE);

  ret = rzv_dmac_channel_configure(RZV_DMAC_TEST_CHANNEL, &config);
  if (ret < 0)
    {
      return ret;
    }

  ret = rzv_dmac_channel_start(RZV_DMAC_TEST_CHANNEL);
  if (ret < 0)
    {
      (void)rzv_dmac_channel_stop(RZV_DMAC_TEST_CHANNEL);
      return ret;
    }

  for (index = 0; index < RZV_DMAC_TEST_POLLS; index++)
    {
      status = rzv_dmac_channel_status(RZV_DMAC_TEST_CHANNEL);
      if (status & (RZV_DMAC_STATUS_END | RZV_DMAC_STATUS_ER))
        {
          break;
        }
    }

  if (index == RZV_DMAC_TEST_POLLS)
    {
      (void)rzv_dmac_channel_stop(RZV_DMAC_TEST_CHANNEL);
      return -ETIMEDOUT;
    }

  printf("DMAC memcpy: status=0x%08lx\n", (unsigned long)status);

  ret = rzv_dmac_channel_stop(RZV_DMAC_TEST_CHANNEL);
  if (ret < 0)
    {
      return ret;
    }

  if (status & RZV_DMAC_STATUS_ER)
    {
      return -EIO;
    }

  if (memcmp(g_src_buffer, g_dst_buffer, sizeof(g_src_buffer)) != 0)
    {
      return -EIO;
    }

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  int ret;

  (void)argc;
  (void)argv;

  ret = rzv_dmac_memcpy_test();
  if (ret < 0)
    {
      printf("FAIL: DMAC memcpy status=%d\n", ret);
      return EXIT_FAILURE;
    }

  printf("PASS: DMAC memcpy\n");
  return EXIT_SUCCESS;
}
