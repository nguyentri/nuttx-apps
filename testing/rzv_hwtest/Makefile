include $(APPDIR)/Make.defs

PROGNAME  = $(CONFIG_TESTING_RZV_HWTEST_PROGNAME)
PRIORITY  = $(CONFIG_TESTING_RZV_HWTEST_PRIORITY)
STACKSIZE = $(CONFIG_TESTING_RZV_HWTEST_STACKSIZE)
MODULE    = $(CONFIG_TESTING_RZV_HWTEST)

# Header lookup for subdir sources

CFLAGS += -I$(APPDIR)/testing/rzv_hwtest

# Core harness (mode-independent)

CSRCS  = core/harness.c

# Per-mode suites

ifeq ($(CONFIG_TESTING_RZV_HWTEST_MODE_UART_LOOPBACK),y)
CSRCS += uart/uart_loopback_test.c
endif

ifeq ($(CONFIG_TESTING_RZV_HWTEST_MODE_SPI_LOOPBACK),y)
CSRCS += spi/spi_loopback_test.c
endif

ifeq ($(CONFIG_TESTING_RZV_HWTEST_MODE_HIL_FULL),y)
CSRCS += hil/mpu9250_test.c
CSRCS += hil/bmp280_test.c
CSRCS += hil/gps_nmea_test.c
CSRCS += hil/tfmini_test.c
CSRCS += hil/hil_full_test.c
endif

MAINSRC = core/hwtest_main.c

include $(APPDIR)/Application.mk
