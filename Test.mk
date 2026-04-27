TEST_CC = gcc
TEST_C_INCLUDE_FLAGS += -I$(UNITY_DIR)/src/ 
# TEST_C_INCLUDE_FLAGS += $(C_INCLUDES)
TEST_C_INCLUDE_FLAGS += -Isrc/ 
TEST_C_INCLUDE_FLAGS += -Ilib/sh2
# TEST_C_INCLUDE_FLAGS += -Iboard/$(BOARD)/Core/Inc

TEST_C_OPT = -O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer
TEST_C_WARNINGS = -Wall -Wextra -Wno-unused-function -Wdate-time 
TEST_C_DEF = -DUNIT_TEST -D_FORTIFY_SOURCE=2 -DUNITY_INCLUDE_DOUBLE
TEST_CFLAGS = $(TEST_C_OPT) $(TEST_C_WARNINGS) $(TEST_C_DEF) $(TEST_C_INCLUDE_FLAGS)
TEST_LDFLAGS = -lm -fsanitize=address,undefined -L $(UNITY_DIR) -lunity

UNITY_DIR := lib/unity
UNITY_SRC := $(UNITY_DIR)/src/unity.c
UNITY = $(UNITY_DIR)/libunity.a

TEST_DIR := test
STUB_DIR := $(TEST_DIR)/stubs
MOCK_DIR := $(TEST_DIR)/mocks
FAKE_DIR := $(TEST_DIR)/fakes
TEST_SRC_DIR := $(TEST_DIR)/src
TEST_BUILD_DIR := $(TEST_DIR)/build

STUB_SRC := $(shell find $(STUB_DIR) -type f -iname '*.c' 2> /dev/null)
MOCK_SRC := $(shell find $(MOCK_DIR) -type f -iname '*.c' 2> /dev/null)
FAKE_SRC := $(shell find $(FAKE_DIR) -type f -iname '*.c' 2> /dev/null)

STUB_OBJ = $(addprefix $(TEST_BUILD_DIR)/,$(patsubst %.c, %.c.o, $(STUB_SRC)))
MOCK_OBJ = $(addprefix $(TEST_BUILD_DIR)/,$(patsubst %.c, %.c.o, $(MOCK_SRC)))
FAKE_OBJ = $(addprefix $(TEST_BUILD_DIR)/,$(patsubst %.c, %.c.o, $(FAKE_SRC)))

TEST_SRC := $(shell find $(TEST_SRC_DIR) -type f -iname '*.c' 2> /dev/null)
TEST_OBJ = $(addprefix $(TEST_BUILD_DIR)/,$(patsubst $(TEST_DIR)/%.c, %.c.o, $(TEST_SRC)))
TEST_BIN = $(patsubst $(TEST_SRC_DIR)/%.c, $(TEST_BUILD_DIR)/%, $(TEST_SRC))

GENERATED_TEST_OBJS = $(TEST_OBJ) $(STUB_OBJ) $(MOCK_OBJ) $(FAKE_OBJ)
TEST_OUT_DIRS :=  $(sort $(dir $(TEST_BIN)) $(dir $(GENERATED_TEST_OBJS)))

# Tools/dependencies
$(UNITY_SRC):
	git submodule update --init --recursive -- $(UNITY_DIR)

$(UNITY_DIR)/Makefile: $(UNITY_SRC)
	@cd $(UNITY_DIR) && cmake -DCMAKE_C_FLAGS=-DUNITY_INCLUDE_DOUBLE .

$(UNITY): $(UNITY_DIR)/Makefile
	@$(MAKE) -C $(UNITY_DIR)

# Test Output Directories
$(TEST_OUT_DIRS):
	$(call print1,Making Folder:,$@)
	@mkdir -p $@

# .c -> .c.o
$(TEST_BUILD_DIR)/%.o: test/% | $(TEST_OUT_DIRS)
	$(call print2,Compiling:,$<,$@)
	@$(TEST_CC) $(TEST_CFLAGS) -MMD -MP -o $@ -c $<

-include $(GENERATED_TEST_OBJS:.o=.d)

# Executable Test Binaries
$(TEST_BUILD_DIR)/%.test: $(TEST_BUILD_DIR)/src/%.test.c.o $(GENERATED_TEST_OBJS) $(TEST_DEP) $(UNITY) | $(TEST_OUT_DIRS)
	$(call print1,Linking executable:,$@)
	@$(TEST_CC) $(TEST_CFLAGS) -o $@ $< $(TEST_DEP) $(UNITY) $(TEST_LDFLAGS)

TEST_RUN = $(addsuffix .run, $(TEST_BIN))

# Run individual test binary
$(TEST_RUN): $(TEST_BUILD_DIR)/%.run: $(TEST_BUILD_DIR)/%
	@./$<

# Run all tests sequentially — fail immediately if any test exits non-zero
test: $(TEST_BIN)
	@fail=0; \
	for t in $(TEST_BIN); do \
		./$$t || fail=1; \
	done; \
	if [ $$fail -ne 0 ]; then \
		echo "$(RED)One or more tests failed.$(NO_COL)"; \
		exit 1; \
	fi
	@$(PRINT) "\n$$(sed "6s|\(.*\)|\1    \\$(GREEN)Success!\\$(NO_COL)|" $(MICROROBOTICS_LOGO))\n\n"

test_clean:
	@$(RM) -rf $(TEST_BUILD_DIR)
	@$(RM) -rf $(UNITY) $(UNITY_DIR)/Makefile

.PHONY: \
	test \
	test_clean \
	$(TEST_RUN)

# test dependencies
TEST_REAL_DEP = 
TEST_TEST_DEP = 
TEST_STUB_DEP = 
TEST_MOCK_DEP = 
TEST_FAKE_DEP = 
TEST_DEP = $(addprefix $(SRC_DIR)/, $(TEST_REAL_DEP)) \
	$(addprefix $(TEST_SRC_DIR)/,  $(patsubst %.o, %.test.o, $(TEST_TEST_DEP))) \
	$(addprefix $(STUB_DIR)/, $(patsubst %.o, %.stub.o, $(TEST_STUB_DEP))) \
	$(addprefix $(MOCK_DIR)/, $(patsubst %.o, %.mock.o, $(TEST_MOCK_DEP))) \
	$(addprefix $(FAKE_DIR)/, $(patsubst %.o, %.fake.o, $(TEST_FAKE_DEP)))

$(TEST_BUILD_DIR)/mission/mission_battery.test: 