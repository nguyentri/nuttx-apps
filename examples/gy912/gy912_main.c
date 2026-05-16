/****************************************************************************
 * apps/examples/gy912/gy912_main.c
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

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Forward declaration of board-level function */
extern int ra8e1_spi_gy912_main(int argc, char *argv[]);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * gy912_main
 *
 * Description:
 *   Main entry point for the GY-912 sensor example.
 *   This is a wrapper that calls the board-level implementation.
 *
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  printf("GY-912 Sensor Test Application\n");
  printf("Calling board-level implementation...\n\n");

  return ra8e1_spi_gy912_main(argc, argv);
}
