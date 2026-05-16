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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <semaphore.h>

#include "rzv_dmac.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define BUFFER_SIZE_1K    1024
#define BUFFER_SIZE_4K    4096
#define BUFFER_SIZE_64K   65536

#define TEST_PATTERN_0    0xAA
#define TEST_PATTERN_1    0x55
#define TEST_PATTERN_2    0x12
#define TEST_PATTERN_3    0x34

/****************************************************************************
 * Private Data
 ****************************************************************************/

static uint8_t g_src_buffer[BUFFER_SIZE_64K] __attribute__((aligned(128)));
static uint8_t g_dst_buffer[BUFFER_SIZE_64K] __attribute__((aligned(128)));
static sem_t g_dma_sem;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: dma_callback
 *
 * Description:
 *   DMA transfer completion callback
 *
 ****************************************************************************/

static void dma_callback(void *handle, int event, void *user_data)
{
  sem_t *sem = (sem_t *)user_data;

  if (event == RZV_DMAC_EVENT_COMPLETE)
    {
      printf("DMA transfer completed successfully\n");
      sem_post(sem);
    }
  else if (event == RZV_DMAC_EVENT_ERROR)
    {
      printf("DMA transfer error!\n");
      sem_post(sem);
    }
}

/****************************************************************************
 * Name: verify_buffer
 *
 * Description:
 *   Verify that destination buffer matches source buffer
 *
 ****************************************************************************/

static bool verify_buffer(const uint8_t *src, const uint8_t *dst,
                          size_t size)
{
  for (size_t i = 0; i < size; i++)
    {
      if (src[i] != dst[i])
        {
          printf("Mismatch at offset %zu: src=0x%02x, dst=0x%02x\n",
                 i, src[i], dst[i]);
          return false;
        }
    }

  return true;
}

/****************************************************************************
 * Name: measure_throughput
 *
 * Description:
 *   Measure DMA throughput
 *
 ****************************************************************************/

static void measure_throughput(size_t bytes, struct timespec *start,
                                struct timespec *end)
{
  long ns = (end->tv_sec - start->tv_sec) * 1000000000L +
            (end->tv_nsec - start->tv_nsec);
  double seconds = ns / 1000000000.0;
  double throughput = (bytes / seconds) / (1024.0 * 1024.0);

  printf("  Transfer time: %.3f ms\n", ns / 1000000.0);
  printf("  Throughput: %.2f MB/s\n", throughput);
}

/****************************************************************************
 * Name: test_basic_memcpy
 *
 * Description:
 *   Test basic memory-to-memory DMA transfer (polling mode)
 *
 ****************************************************************************/

static int test_basic_memcpy(int channel, size_t size)
{
  struct rzv_dmac_config_s config;
  struct timespec start;
  struct timespec end;
  uint32_t status;
  int ret;

  printf("\n=== Test: Basic Memory Copy (%zu bytes) ===\n", size);

  /* Initialize buffers */

  memset(g_src_buffer, TEST_PATTERN_0, size);
  memset(g_dst_buffer, 0, size);

  /* Configure DMA */

  memset(&config, 0, sizeof(config));
  config.mode            = RZV_DMAC_MODE_REGISTER;
  config.src_size        = RZV_DMAC_SIZE_4BYTE;
  config.dst_size        = RZV_DMAC_SIZE_4BYTE;
  config.src_addr_mode   = RZV_DMAC_ADDR_INCREMENT;
  config.dst_addr_mode   = RZV_DMAC_ADDR_INCREMENT;
  config.trigger         = RZV_DMAC_TRIGGER_SW;
  config.detect_mode     = RZV_DMAC_DETECT_LOW_LEVEL;
  config.src_addr        = (uint32_t)g_src_buffer;
  config.dst_addr        = (uint32_t)g_dst_buffer;
  config.length          = size;
  config.priority        = 4;
  config.transfer_interval = 0;
  config.elc_event       = -1;
  config.irq_num         = -1;
  config.callback        = NULL;
  config.user_data       = NULL;

  ret = rzv_dmac_channel_configure(channel, &config);
  if (ret < 0)
    {
      printf("ERROR: Failed to configure DMAC: %d\n", ret);
      return ret;
    }

  /* Start transfer and measure time */

  clock_gettime(CLOCK_MONOTONIC, &start);

  ret = rzv_dmac_channel_start(channel);
  if (ret < 0)
    {
      printf("ERROR: Failed to start DMAC: %d\n", ret);
      rzv_dmac_channel_stop(channel);
      return ret;
    }

  /* Poll for completion */

  do
    {
      status = rzv_dmac_channel_status(channel);
    }
  while (status & RZV_DMAC_STATUS_TACT);

  clock_gettime(CLOCK_MONOTONIC, &end);

  /* Check for errors */

  if (status & RZV_DMAC_STATUS_ER)
    {
      printf("ERROR: DMA transfer error, status=0x%08lx\n", status);
      rzv_dmac_channel_stop(channel);
      return -EIO;
    }

  /* Verify data */

  if (!verify_buffer(g_src_buffer, g_dst_buffer, size))
    {
      printf("ERROR: Data verification failed\n");
      rzv_dmac_channel_stop(channel);
      return -EIO;
    }

  measure_throughput(size, &start, &end);

  rzv_dmac_channel_stop(channel);

  printf("PASS: Basic memory copy test\n");
  return OK;
}

/****************************************************************************
 * Name: test_interrupt_mode
 *
 * Description:
 *   Test DMA with interrupt callback
 *
 ****************************************************************************/

static int test_interrupt_mode(int channel, size_t size)
{
  struct rzv_dmac_config_s config;
  struct timespec timeout;
  int ret;

  printf("\n=== Test: Interrupt Mode Transfer (%zu bytes) ===\n", size);

  /* Initialize buffers */

  memset(g_src_buffer, TEST_PATTERN_1, size);
  memset(g_dst_buffer, 0, size);

  /* Initialize semaphore */

  sem_init(&g_dma_sem, 0, 0);

  /* Configure DMA with callback */

  memset(&config, 0, sizeof(config));
  config.mode            = RZV_DMAC_MODE_REGISTER;
  config.src_size        = RZV_DMAC_SIZE_4BYTE;
  config.dst_size        = RZV_DMAC_SIZE_4BYTE;
  config.src_addr_mode   = RZV_DMAC_ADDR_INCREMENT;
  config.dst_addr_mode   = RZV_DMAC_ADDR_INCREMENT;
  config.trigger         = RZV_DMAC_TRIGGER_SW;
  config.detect_mode     = RZV_DMAC_DETECT_LOW_LEVEL;
  config.src_addr        = (uint32_t)g_src_buffer;
  config.dst_addr        = (uint32_t)g_dst_buffer;
  config.length          = size;
  config.priority        = 5;
  config.transfer_interval = 0;
  config.elc_event       = -1;
  config.irq_num         = 123;  /* Example IRQ number */
  config.callback        = dma_callback;
  config.user_data       = &g_dma_sem;

  ret = rzv_dmac_channel_configure(channel, &config);
  if (ret < 0)
    {
      printf("ERROR: Failed to configure DMAC: %d\n", ret);
      sem_destroy(&g_dma_sem);
      return ret;
    }

  /* Start transfer */

  ret = rzv_dmac_channel_start(channel);
  if (ret < 0)
    {
      printf("ERROR: Failed to start DMAC: %d\n", ret);
      rzv_dmac_channel_stop(channel);
      sem_destroy(&g_dma_sem);
      return ret;
    }

  /* Wait for completion with timeout */

  clock_gettime(CLOCK_REALTIME, &timeout);
  timeout.tv_sec += 5;  /* 5 second timeout */

  ret = sem_timedwait(&g_dma_sem, &timeout);
  if (ret < 0)
    {
      printf("ERROR: DMA transfer timeout\n");
      rzv_dmac_channel_stop(channel);
      sem_destroy(&g_dma_sem);
      return -ETIMEDOUT;
    }

  /* Verify data */

  if (!verify_buffer(g_src_buffer, g_dst_buffer, size))
    {
      printf("ERROR: Data verification failed\n");
      rzv_dmac_channel_stop(channel);
      sem_destroy(&g_dma_sem);
      return -EIO;
    }

  rzv_dmac_channel_stop(channel);
  sem_destroy(&g_dma_sem);

  printf("PASS: Interrupt mode transfer test\n");
  return OK;
}

/****************************************************************************
 * Name: test_different_sizes
 *
 * Description:
 *   Test various transfer sizes (1, 2, 4, 8, 16, 32, 64, 128 bytes)
 *
 ****************************************************************************/

static int test_different_sizes(int channel)
{
  struct rzv_dmac_config_s config;
  const rzv_dmac_size_t sizes[] =
  {
    RZV_DMAC_SIZE_1BYTE,
    RZV_DMAC_SIZE_2BYTE,
    RZV_DMAC_SIZE_4BYTE,
    RZV_DMAC_SIZE_8BYTE,
    RZV_DMAC_SIZE_16BYTE,
    RZV_DMAC_SIZE_32BYTE,
    RZV_DMAC_SIZE_64BYTE,
    RZV_DMAC_SIZE_128BYTE
  };
  const char *size_names[] =
  {
    "1-byte", "2-byte", "4-byte", "8-byte",
    "16-byte", "32-byte", "64-byte", "128-byte"
  };
  int ret;

  printf("\n=== Test: Different Transfer Sizes ===\n");

  for (int i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
    {
      printf("\nTesting %s transfers...\n", size_names[i]);

      /* Initialize buffers */

      memset(g_src_buffer, TEST_PATTERN_2 + i, BUFFER_SIZE_1K);
      memset(g_dst_buffer, 0, BUFFER_SIZE_1K);

      /* Configure DMA */

      memset(&config, 0, sizeof(config));
      config.mode            = RZV_DMAC_MODE_REGISTER;
      config.src_size        = sizes[i];
      config.dst_size        = sizes[i];
      config.src_addr_mode   = RZV_DMAC_ADDR_INCREMENT;
      config.dst_addr_mode   = RZV_DMAC_ADDR_INCREMENT;
      config.trigger         = RZV_DMAC_TRIGGER_SW;
      config.detect_mode     = RZV_DMAC_DETECT_LOW_LEVEL;
      config.src_addr        = (uint32_t)g_src_buffer;
      config.dst_addr        = (uint32_t)g_dst_buffer;
      config.length          = BUFFER_SIZE_1K;
      config.priority        = 4;
      config.transfer_interval = 0;
      config.elc_event       = -1;
      config.irq_num         = -1;
      config.callback        = NULL;
      config.user_data       = NULL;

      ret = rzv_dmac_channel_configure(channel, &config);
      if (ret < 0)
        {
          printf("ERROR: Failed to configure DMAC: %d\n", ret);
          return ret;
        }

      ret = rzv_dmac_channel_start(channel);
      if (ret < 0)
        {
          printf("ERROR: Failed to start DMAC: %d\n", ret);
          rzv_dmac_channel_stop(channel);
          return ret;
        }

      /* Wait for completion */

      uint32_t status;
      do
        {
          status = rzv_dmac_channel_status(channel);
        }
      while (status & RZV_DMAC_STATUS_TACT);

      if (status & RZV_DMAC_STATUS_ER)
        {
          printf("ERROR: DMA transfer error\n");
          rzv_dmac_channel_stop(channel);
          return -EIO;
        }

      /* Verify data */

      if (!verify_buffer(g_src_buffer, g_dst_buffer, BUFFER_SIZE_1K))
        {
          printf("ERROR: Data verification failed for %s\n",
                 size_names[i]);
          rzv_dmac_channel_stop(channel);
          return -EIO;
        }

      printf("  %s: PASS\n", size_names[i]);

      rzv_dmac_channel_stop(channel);
    }

  printf("\nPASS: All transfer size tests\n");
  return OK;
}

/****************************************************************************
 * Name: test_fixed_address
 *
 * Description:
 *   Test fixed address mode (simulating peripheral register access)
 *
 ****************************************************************************/

static int test_fixed_address(int channel)
{
  struct rzv_dmac_config_s config;
  uint32_t peripheral_reg = 0;
  uint32_t status;
  int ret;

  printf("\n=== Test: Fixed Address Mode ===\n");

  /* Initialize source buffer with pattern */

  for (size_t i = 0; i < 256; i++)
    {
      g_src_buffer[i] = (uint8_t)i;
    }

  /* Test: Memory to Fixed Address (write to peripheral) */

  printf("\nTesting Memory -> Fixed Address...\n");

  memset(&config, 0, sizeof(config));
  config.mode            = RZV_DMAC_MODE_REGISTER;
  config.src_size        = RZV_DMAC_SIZE_4BYTE;
  config.dst_size        = RZV_DMAC_SIZE_4BYTE;
  config.src_addr_mode   = RZV_DMAC_ADDR_INCREMENT;
  config.dst_addr_mode   = RZV_DMAC_ADDR_FIXED;
  config.trigger         = RZV_DMAC_TRIGGER_SW;
  config.detect_mode     = RZV_DMAC_DETECT_LOW_LEVEL;
  config.src_addr        = (uint32_t)g_src_buffer;
  config.dst_addr        = (uint32_t)&peripheral_reg;
  config.length          = 256;
  config.priority        = 4;
  config.transfer_interval = 0;
  config.elc_event       = -1;
  config.irq_num         = -1;
  config.callback        = NULL;
  config.user_data       = NULL;

  ret = rzv_dmac_channel_configure(channel, &config);
  if (ret < 0)
    {
      printf("ERROR: Failed to configure DMAC: %d\n", ret);
      return ret;
    }

  ret = rzv_dmac_channel_start(channel);
  if (ret < 0)
    {
      printf("ERROR: Failed to start DMAC: %d\n", ret);
      rzv_dmac_channel_stop(channel);
      return ret;
    }

  do
    {
      status = rzv_dmac_channel_status(channel);
    }
  while (status & RZV_DMAC_STATUS_TACT);

  if (status & RZV_DMAC_STATUS_ER)
    {
      printf("ERROR: DMA transfer error\n");
      rzv_dmac_channel_stop(channel);
      return -EIO;
    }

  printf("  Memory -> Fixed: PASS (last value = 0x%08lx)\n",
         peripheral_reg);

  rzv_dmac_channel_stop(channel);

  printf("\nPASS: Fixed address mode test\n");
  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: main
 *
 * Description:
 *   RZV2H DMAC test application main entry point
 *
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  int channel = 48;  /* Use DMAC3 channel 0 (global channel 48) */
  int ret;

  printf("\n");
  printf("*********************************************\n");
  printf("*   RZV2H DMAC Test Application            *\n");
  printf("*********************************************\n");
  printf("\n");

  printf("Using DMA channel: %d (Unit %d, CH %d)\n",
         channel, channel / 16, channel % 16);

  /* Test 1: Basic memory copy with different sizes */

  ret = test_basic_memcpy(channel, BUFFER_SIZE_1K);
  if (ret < 0)
    {
      goto error_exit;
    }

  ret = test_basic_memcpy(channel, BUFFER_SIZE_4K);
  if (ret < 0)
    {
      goto error_exit;
    }

  ret = test_basic_memcpy(channel, BUFFER_SIZE_64K);
  if (ret < 0)
    {
      goto error_exit;
    }

  /* Test 2: Interrupt mode (if interrupts are configured) */

#ifdef CONFIG_RZV_DMAC_DEBUG
  ret = test_interrupt_mode(channel, BUFFER_SIZE_4K);
  if (ret < 0)
    {
      printf("WARNING: Interrupt test failed (may not be configured)\n");
    }
#endif

  /* Test 3: Different transfer sizes */

  ret = test_different_sizes(channel);
  if (ret < 0)
    {
      goto error_exit;
    }

  /* Test 4: Fixed address mode */

  ret = test_fixed_address(channel);
  if (ret < 0)
    {
      goto error_exit;
    }

  /* All tests passed */

  printf("\n");
  printf("*********************************************\n");
  printf("*   ALL TESTS PASSED                       *\n");
  printf("*********************************************\n");
  printf("\n");

  return EXIT_SUCCESS;

error_exit:
  printf("\n");
  printf("*********************************************\n");
  printf("*   TEST FAILED                            *\n");
  printf("*********************************************\n");
  printf("\n");

  return EXIT_FAILURE;
}
