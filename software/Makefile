BUILD_DIR = ./build

INCLUDE_DIR = ./include

CC = gcc
CC_OPT = -I$(INCLUDE_DIR)

MINI_AUDIO_OPT = -ldl -lpthread -lm

include sources.mk

BUILD_TARGETS := $(patsubst %.c, $(BUILD_DIR)/%.o, $(SRC_FILES))
BUILD_DEPENDENCIES := $(patsubst %.c, $(BUILD_DIR)/%.d, $(SRC_FILES))

-include $(BUILD_DEPENDENCIES)

# Build objects from the src files

$(BUILD_DIR):
	mkdir -p $@
	mkdir -p $@/src

$(BUILD_DIR)/%.o: %.c
	$(CC) $(CC_OPT) -c -MMD -MP $(RELEASE) -o $@ $<

# Build executables

test: $(BUILD_DIR) $(BUILD_TARGETS) 
	gcc -o $(BUILD_DIR)/test $(BUILD_TARGETS) test.c $(CC_OPT)

# Misc

HEADER_FILES = $(shell find ./include -name "*.h")

clean:
	@rm -rf $(BUILD_DIR)

format:
	clang-format --dry-run $(C_FILES) $(HEADER_FILES) --verbose -Werror

format-hard:
	clang-format -i --verbose $(C_FILES) $(HEADER_FILES)

.PHONY: format format-check test