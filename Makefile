BOARD ?= v3_1
DEBUG ?= 1

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
ifeq ($(BOARD), nucleo)
	CPU = -mcpu=cortex-m33
	FPU = -mfpu=fpv4-sp-d16
	FLOAT-ABI = -mfloat-abi=hard
	MCU = $(CPU) -mthumb $(FPU) $(FLOAT-ABI)
	C_DEFS += -DSTM32U5A5xx
	C_DEFS += -DHW_VERSION=0
endif

ifeq ($(BOARD), v3_1)
	CPU = -mcpu=cortex-m33
	FPU = -mfpu=fpv4-sp-d16
	FLOAT-ABI = -mfloat-abi=hard
	MCU = $(CPU) -mthumb $(FPU) $(FLOAT-ABI)
	C_DEFS += -DSTM32U595xx
	C_DEFS += -DHW_VERSION=1
endif

ifeq ($(BOARD), msh)
	CPU = -mcpu=cortex-m33
	FPU = -mfpu=fpv4-sp-d16
	FLOAT-ABI = -mfloat-abi=hard
	MCU = $(CPU) -mthumb $(FPU) $(FLOAT-ABI)
	C_DEFS += -DSTM32U595xx
	C_DEFS += -DHW_VERSION=2
endif

C_DEFS +=  \
-DFX_INCLUDE_USER_DEFINE_FILE \
-DUSE_HAL_DRIVER

ifeq ($(DEBUG), 1)
	COPT = -Og -g -gdwarf-2
	C_DEFS += -DDEBUG
else
	COPT = -O2
endif

# Generate dependency information
C_INCLUDES += $(addprefix -I,$(shell find board/$(BOARD) -type d \( -iname 'inc' -o -iname 'include' -o -iwholename '*/inc/legacy' -o -iname 'app' \) 2> /dev/null))
C_INCLUDES += -Ilib/tinyusb/src
C_INCLUDES += -Ilib/sh2
C_INCLUDES += -Ilib/minmea
C_INCLUDES += -Isrc
C_INCLUDES += -Iboard/$(BOARD)/FileX/Target
CFLAGS += $(MCU) $(C_DEFS) $(C_INCLUDES) $(COPT) -Wall -fdata-sections -ffunction-sections
CFLAGS += -MMD -MP -MF"$(@:%.o=%.d)"
CFLAGS += -DCFG_TUSB_CONFIG_FILE="\"usb/tusb_config.h\""

# link script
LDSCRIPT = $(addprefix -T,$(shell find board/$(BOARD) -type f -iname '*_FLASH.ld'))

# libraries
LIBS = -lc -lm -lnosys 
LIBDIR = 
LDFLAGS = $(MCU) -u _printf_float $(LDSCRIPT) $(LIBDIR) $(LIBS) -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections
# LDFLAGS += -specs=nano.specs

### Files and Directories ###

# source files
SRC_DIR = src
VERSION_H := $(SRC_DIR)/version.h

C_SRCS = $(shell find src -type f -iname '*.c' 2> /dev/null)
C_SRCS += $(shell find board/$(BOARD) -type f -iname '*.c' 2> /dev/null)
C_SRCS += $(shell find lib/tinyusb/src -type f -iname '*.c' 2> /dev/null) #tinyusb
C_SRCS += $(shell find lib/sh2 -type f -iname '*.c' 2> /dev/null) #sh2 for BNO08x
C_SRCS += lib/minmea/minmea.c #nmea parser

ASM_SRCS = $(shell find board/$(BOARD) src -type f -iname '*.s' 2> /dev/null)
C_OBJS = $(addprefix $(BUILD_DIR)/,$(patsubst %.c, %.c.o, $(C_SRCS)))
ASM_OBJS = $(addprefix $(BUILD_DIR)/,$(patsubst %.s, %.s.o, $(ASM_SRCS)))
ALL_OBJS = $(C_OBJS) $(ASM_OBJS)

# Build folders
ifeq ($(DEBUG), 1)
	BUILD_DIR = $(BUILD_ROOT)/$(BOARD)/debug
else
	BUILD_DIR = $(BUILD_ROOT)/$(BOARD)/release
endif
C_BUILD_DIRS := $(sort $(dir $(C_OBJS)))
ASM_BUILD_DIRS := $(sort $(dir $(ASM_OBJS)))
ALL_DIRS := $(BUILD_DIR) $(C_BUILD_DIRS) $(ASM_BUILD_DIRS)


### Rules ###

# default target
DOCKER_IMAGE = stm32_build_img

all: $(C_SRCS_FILE) $(BUILD_DIR)/$(TARGET).bin $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex


build: $(DOCKER_IMAGE)
	$(call print0, Running make inside docker)
	docker run --rm \
		--user $(USER):$(USER_GROUP) \
		--volume $(PWD):/ceti-firmware \
		$(DOCKER_IMAGE) \
			make

# src/version_hw.h
$(VERSION_H):
	$(call print2,Updating version info:,$(GIT_VERSION_INFO),$@)
	@echo "#ifndef CETI_WHALE_TAG_VER_H" > $@
	@echo "#define CETI_WHALE_TAG_VER_H" >> $@
	@echo "#define FW_VERSION_TEXT \"$(GIT_VERSION_INFO)\"" >> $@
	@echo "#endif // CETI_WHALE_TAG_VERSIONING_H" >> $@

# mkdirs
$(ALL_DIRS):
	$(call print1,Making Folder:,$@)
	@$(MKDIR) -p $@

# .s -> .o
$(BUILD_DIR)/%.s.o : %.s | $(ASM_BUILD_DIRS)
	$(call print2,Assembling:,$<,$@)
	@$(CC) -x assembler-with-cpp -c $(CFLAGS) $< -o $@
	
#main.c -> main.o
$(BUILD_DIR)/%/main.c.o : %/main.c src/config.h | $(C_BUILD_DIRS)
	$(call print2,Compiling:,$<,$@)
	@$(CC) -c $(CFLAGS) $< -o $@ 

# .c -> .o
$(BUILD_DIR)/%.c.o : %.c | $(C_BUILD_DIRS)
	$(call print2,Compiling:,$<,$@)
	@$(CC) -c $(CFLAGS) $< -o $@ 

$(BUILD_DIR)/$(TARGET).elf: $(VERSION_H) $(ALL_OBJS) | $(BUILD_DIR)
	$(call print1,Linking elf:,$@)
	@$(CC) $^ $(LDFLAGS) -o $@
	$(SZ) $@
	@$(PRINT) "\n$$(cat logo.ansi.txt)\n"	

$(BUILD_DIR)/$(TARGET).hex: $(BUILD_DIR)/$(TARGET).elf 
	$(call print1,Creating hex:,$@)
	@$(CP) -O ihex $< $@

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf 
	$(call print1,Creating bin:,$@)
	@$(CP) -O binary -S $< $@

flash: $(BUILD_DIR)/$(TARGET).elf
	$(call print0, Flashing via stlink)
	STM32_Programmer_CLI --connect port=swd --write $< --go

clean: 
	$(call print0, Cleaning build artifacts)
	@$(RM) -rf $(BUILD_ROOT)



lint:
	docker run \
		-e RUN_LOCAL=true \
		-e VALIDATE_CLANG_FORMAT=true \
		-e LINTER_RULES_PATH=.github/linters \
		-e DEFAULT_BRANCH=main \
		-e FILTER_REGEX_EXCLUDE='.*(board|lib)/.*' \
		-v $(PWD)/.:/tmp/lint \
		--rm ghcr.io/super-linter/super-linter:latest

lint_fix:
	docker run \
		-e RUN_LOCAL=true \
		-e VALIDATE_CPP=false \
		-e VALIDATE_DOCKERFILE_HADOLINT=false \
		-e VALIDATE_GITHUB_ACTIONS=false \
		-e VALIDATE_JSCPD=false \
		-e VALIDATE_MARKDOWN=false \
		-e VALIDATE_NATURAL_LANGUAGE=false \
		-e VALIDATE_YAML=false \
		-e VALIDATE_GITLEAKS=false \
		-e FIX_CLANG_FORMAT=true \
		-e LINTER_RULES_PATH=.github/linters \
		-e DEFAULT_BRANCH=main \
		-e FILTER_REGEX_EXCLUDE='.*(board|lib)/.*' \
		-v $(shell pwd):/tmp/lint \
		--rm ghcr.io/super-linter/super-linter:latest

include Test.mk

# Per file specific flags
$(BUILD_DIR)/lib/minmea/minmea.c.o: CFLAGS += -Dtimegm=mktime

$(DOCKER_IMAGE): Dockerfile packages.txt
	$(call print0, Building docker image)
	docker build -t $(DOCKER_IMAGE) .


.PHONY: all \
	release \
	debug \
	docker \
	clean \
	flash \
	build \
	$(DOCKER_IMAGE) \
	$(VERSION_H) 
