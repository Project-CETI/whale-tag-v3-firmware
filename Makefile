
BOARD ?= v3_2
SUPPORTED_BOARDS := v3_1_1 v3_2

MODE ?= release
SUPPORTED_MODES := debug bench release

BUILD_ROOT = build

TARGET = ceti-whale-tag

ifeq ($(OS),Windows_NT)
	USER := 1000
	USER_GROUP := 1000
#	TEST := $(shell Get-ChildItem -Filter *.c -Recurse $PWD)	
# 	FIND := <some Windows-compatible find>
else
	PWD := $(shell pwd)
	USER := $(shell id -u)
	USER_GROUP := $(shell id -g)
endif


### Tools ###
RM := rm
MKDIR := mkdir
include print.mk

CROSS := arm-none-eabi-
CC := $(CROSS)gcc
CP := $(CROSS)objcopy
SZ := $(CROSS)size

### Versioning ###
GIT_VERSION_INFO = $(shell if !(git log -1 --format="%at: %h - %s"); then echo "unknown"; fi)


### C Compilation settings ###
# Board specific 
CPU = -mcpu=cortex-m33
FPU = -mfpu=fpv4-sp-d16
FLOAT-ABI = -mfloat-abi=hard
MCU = $(CPU) -mthumb $(FPU) $(FLOAT-ABI)

### Hardware specific definitions
ifeq ($(BOARD), v3_1_1)
	C_DEFS += -DHW_VERSION=2
else ifeq ($(BOARD), v3_2)
	C_DEFS += -DHW_VERSION=3
else
$(error Target hardware $(BOARD) not supported. Supported hardware: $(SUPPORTED_BOARDS)) 
endif

C_DEFS += -DSTM32U595xx
# link script
LDSCRIPT = -Tboard/STM32U595xx_FLASH.ld

C_DEFS +=  \
-DFX_INCLUDE_USER_DEFINE_FILE \
-DUSE_HAL_DRIVER

ifeq ($(MODE), debug) 
COPT = -Og -g -gdwarf-2
C_DEFS += -DDEBUG
else ifeq ($(MODE), bench)
COPT = -O3
C_DEFS += -DBENCHMARK
CFLAGS += -finstrument-functions
CFLAGS += -finstrument-functions-exclude-file-list=board/,profile,lib/
else ifeq ($(MODE), release)
COPT = -O3
else
$(error Unknown compilation mode $(MODE). Supported modes: $(SUPPORTED_MODES))
endif

# Generate dependency information
C_INCLUDES += $(addprefix -I,$(shell find board/$(BOARD) -type d \( -iname 'inc' -o -iname 'include' -o -iwholename '*/inc/legacy' -o -iname 'app' \) 2> /dev/null))
C_INCLUDES += -Iboard/$(BOARD)/Core/Inc
C_INCLUDES += -Iboard/$(BOARD)/Filex/App
C_INCLUDES += -Iboard/$(BOARD)/Filex/Target
C_INCLUDES += -Ilib/stm32-mw-filex/ports/generic/inc
C_INCLUDES += -Ilib/stm32-mw-filex/common/inc

C_INCLUDES += -Ilib/cmsis_device_u5/Include
C_INCLUDES += -Ilib/stm32u5xx-hal-driver/Inc
C_INCLUDES += -Ilib/stm32u5xx-hal-driver/Inc/Legacy
C_INCLUDES += -Ilib/CMSIS_5/CMSIS/Core/Include

C_INCLUDES += -Ilib/tinyusb/src
C_INCLUDES += -Ilib/sh2
C_INCLUDES += -Isrc
C_INCLUDES += -Iboard/$(BOARD)/FileX/Target
CFLAGS += $(MCU) $(C_DEFS) $(C_INCLUDES) $(COPT) -Wall -fdata-sections -ffunction-sections
# CFLAGS += -MMD -MP -MF"$(@:%.o=%.d)"
CFLAGS += -DCFG_TUSB_CONFIG_FILE="\"usb/tusb_config.h\""
CFLAGS += -funsigned-char -fshort-enums

# libraries
LIBS = -lc -lm -lnosys 
LIBDIR = 
LDFLAGS = $(MCU) -u _printf_float $(LDSCRIPT) $(LIBDIR) $(LIBS) -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections
# LDFLAGS += -specs=nano.specs

### Files and Directories ###

# source files
SRC_DIR = src
VERSION_H := $(SRC_DIR)/version.h
AOP_H := $(SRC_DIR)/satellite/aop.h

USER_C_SRCS = $(shell find src -type f -iname '*.c' 2> /dev/null)
C_SRCS = $(USER_C_SRCS)
C_SRCS += $(shell find board/$(BOARD) -type f -iname '*.c' 2> /dev/null)
C_SRCS += $(shell find lib/stm32u5xx-hal-driver/Src -type f -iname '*.c' -not -name '*_template.c' 2> /dev/null)
C_SRCS += $(shell find lib/stm32-mw-filex/common/src -type f -iname '*.c' 2> /dev/null)
C_SRCS += lib/stm32-mw-filex/common/drivers/fx_stm32_sd_driver.c

# lib/tinyusb
C_SRCS += \
$(shell find lib/tinyusb/src/class 	-type f -iname '*.c' 2> /dev/null) \
$(shell find lib/tinyusb/src/common -type f -iname '*.c' 2> /dev/null) \
$(shell find lib/tinyusb/src/device -type f -iname '*.c' 2> /dev/null) \
$(shell find lib/tinyusb/src/host 	-type f -iname '*.c' 2> /dev/null) \
$(shell find lib/tinyusb/src/typec 	-type f -iname '*.c' 2> /dev/null) \
$(shell find lib/tinyusb/src/portable/synopsys 	-type f -iname '*.c' 2> /dev/null) \
lib/tinyusb/src/tusb.c

# lib/sh2 (for imu)
C_SRCS += $(shell find lib/sh2 -type f -iname '*.c' 2> /dev/null) #sh2 for BNO08x

# C_SRCS += lib/minmea/minmea.c #nmea parser

ASM_SRCS = $(shell find board/$(BOARD) src -type f -iname '*.s' 2> /dev/null)
C_OBJS = $(addprefix $(BUILD_DIR)/,$(patsubst %.c, %.c.o, $(C_SRCS)))
ASM_OBJS = $(addprefix $(BUILD_DIR)/,$(patsubst %.s, %.s.o, $(ASM_SRCS)))
ALL_OBJS = $(C_OBJS) $(ASM_OBJS)

C_DEPS = $(addprefix $(BUILD_DIR)/,$(patsubst %.c, %.d, $(C_SRCS)))

# Build folders
BUILD_DIR = $(BUILD_ROOT)/$(BOARD)/$(MODE)
C_BUILD_DIRS := $(sort $(dir $(C_OBJS)))
ASM_BUILD_DIRS := $(sort $(dir $(ASM_OBJS)))
ALL_DIRS := $(BUILD_DIR) $(C_BUILD_DIRS) $(ASM_BUILD_DIRS)

DOCKER_IMAGE = stm32_build_img

# default target
all: .gitmodules_updated $(C_SRCS_FILE) $(BUILD_DIR)/$(TARGET).bin $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex

# default target insider docker
build: docker_all

# src/version_hw.h
$(VERSION_H):
	$(call print2,Updating version info:,$(GIT_VERSION_INFO),$@)
	@echo "#ifndef CETI_WHALE_TAG_VER_H" > $@
	@echo "#define CETI_WHALE_TAG_VER_H" >> $@
	@echo "#define FW_VERSION_TEXT \"$(GIT_VERSION_INFO)\"" >> $@
	@echo "#define FW_COMPILATION_DATE \"$$(date)\"" >> $@
	@echo "#endif // CETI_WHALE_TAG_VERSIONING_H" >> $@

#update git submodules
.gitmodules_updated: .gitmodules
	$(call print0,Updating git submodules)
	@git submodule update --init --recursive
	@touch .gitmodules_updated

# mkdirs
$(ALL_DIRS):
	$(call print1,Making Folder:,$@)
	@$(MKDIR) -p $@

# satellite header
$(AOP_H): 
	$(call print1,Generating:,$@)
	@./tools/update_aop.sh

# .s -> .o
$(BUILD_DIR)/%.s.o : %.s | $(ASM_BUILD_DIRS)
	$(call print2,Assembling:,$<,$@)
	@$(CC) -x assembler-with-cpp -c $(CFLAGS) $< -o $@
	
# main.c -> main.o
$(BUILD_DIR)/%/main.c.o : %/main.c src/config.h | $(C_BUILD_DIRS)
	$(call print2,Compiling:,$<,$@)
	@$(CC) -c $(CFLAGS) $< -o $@ -MMD -MP -MF $(BUILD_DIR)/$*/main.d

# .c -> .o
$(BUILD_DIR)/%.c.o : %.c | $(C_BUILD_DIRS)
	$(call print2,Compiling:,$<,$@)
	@$(CC) -c $(CFLAGS) $< -o $@ -MMD -MP -MF $(BUILD_DIR)/$*.d

# .o -> .elf
$(BUILD_DIR)/$(TARGET).elf: $(VERSION_H) $(ALL_OBJS) | $(BUILD_DIR)
	$(call print1,Linking elf:,$@)
	@$(CC) $^ $(LDFLAGS) -o $@
	$(SZ) $@
	$(call logo_with_text,Build Complete!)	

# .elf -> .hex
$(BUILD_DIR)/$(TARGET).hex: $(BUILD_DIR)/$(TARGET).elf 
	$(call print1,Creating hex:,$@)
	@$(CP) -O ihex $< $@

# .elf -> .bin
$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf 
	$(call print1,Creating bin:,$@)
	@$(CP) -O binary -S $< $@

# Per file specific flags
# $(BUILD_DIR)/lib/minmea/minmea.c.o: CFLAGS += -Dtimegm=mktime
ifeq ($(MODE), bench)
# BENCHED_C_SOURCE = $(USER_C_OBJS)
# BENCHED_C_OBJS = $(addprefix $(BUILD_DIR)/,$(addsuffix .o,$(BENCHED_C_SOURCE)))
# $(USER_C_OBJS): CFLAGS += -finstrument-functions
endif

# ADDITIONAL DEPENDENCIES
-include $(C_DEPS)

src/satellite/argos_tx_mgr.c: $(AOP_H)


# Unit testing framework
include Test.mk

flash: # $(BUILD_DIR)/$(TARGET).elf
	$(call print0, Flashing via stlink)
	STM32_Programmer_CLI --connect port=swd --write $(BUILD_DIR)/$(TARGET).elf --go
	$(call logo_with_text,Tag Flashed!)	

clean: test_clean
	$(call print0, Cleaning build artifacts)
	@$(RM) -rf $(BUILD_ROOT)
	@$(RM) -f $(AOP_H)
	@$(RM) -f .gitmodules_updated

LINT_DIRS := src test
LINT_EXCLUDES := 
LINT_FILES := $(shell find $(LINT_DIRS) -type f \( -iname '*.c' -o -iname '*.h' \) 2> /dev/null)
LINT_FILES := $(filter-out $(LINT_EXCLUDES),$(LINT_FILES))
CLANG_FORMAT ?= clang-format
CLANG_FORMAT_STYLE := --style=file:.github/linters/.clang-format

lint:
	$(call print0, Checking source formatting)
	@$(CLANG_FORMAT) $(CLANG_FORMAT_STYLE) --dry-run --Werror $(LINT_FILES)

lint_fix:
	$(call print0, Fixing source formatting)
	@$(CLANG_FORMAT) $(CLANG_FORMAT_STYLE) -i $(LINT_FILES)

docker_%: $(DOCKER_IMAGE) FORCE
	$(call print0, Running 'make $*' inside docker)
	docker run --rm \
		--user $(USER):$(USER_GROUP) \
		--volume $(PWD):/ceti-firmware \
		$(DOCKER_IMAGE) \
			"make $(MAKEFLAGS) $*"

$(DOCKER_IMAGE): Dockerfile packages.txt
	$(call print0, Building docker image)
	docker build -t $(DOCKER_IMAGE) .

# add this as a dependency to force the dependent target to build
FORCE:

.PHONY: all \
	FORCE \
	build \
	clean \
	debug \
	docker \
	flash \
	lint \
	lint_fix \
	release \
	deps \
	$(DOCKER_IMAGE) \
	$(VERSION_H)

