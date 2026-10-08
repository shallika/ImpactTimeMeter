SDK_ROOT ?= $(LOCALAPPDATA)/Nordic/nRF5_SDK_17.1.0
ARM_GCC_ROOT ?= $(LOCALAPPDATA)/Arduino15/packages/Seeeduino/tools/arm-none-eabi-gcc/9-2019q4/bin

override SDK_ROOT := $(subst \,/,$(SDK_ROOT))
override GNU_INSTALL_ROOT := $(patsubst %/,%,$(subst \,/,$(ARM_GCC_ROOT)))/
PROJECT_ROOT := $(patsubst %/,%,$(dir $(abspath $(firstword $(MAKEFILE_LIST)))))
SDK_TEMPLATE := $(SDK_ROOT)/examples/ble_peripheral/ble_app_template/pca10056/s140/armgcc

ifeq ($(subst \,/,$(CURDIR)),$(SDK_TEMPLATE))
override PROJ_DIR := $(PROJECT_ROOT)
override OUTPUT_DIRECTORY := $(shell powershell.exe -NoProfile -Command "$$base=New-Object System.Uri('$(SDK_TEMPLATE)/');$$target=New-Object System.Uri('$(PROJECT_ROOT)/build/');$$base.MakeRelativeUri($$target).ToString()")

SRC_FILES += \
  $(PROJ_DIR)/timeslot.c \
  $(PROJ_DIR)/ble_impact_service.c

include $(SDK_TEMPLATE)/Makefile
else
.PHONY: all default
all default:
	$(MAKE) -C "$(SDK_TEMPLATE)" -f "$(PROJECT_ROOT)/Makefile" nrf52840_xxaa
endif
