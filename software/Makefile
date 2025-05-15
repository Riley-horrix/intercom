BUILD_DIR = ./build

INCLUDES = -I./include -I./lib

CC = gcc

MINI_AUDIO_OPT_LINUX = -ldl -lpthread -lm 
MINI_AUDIO_OPT_GCC   = -msse2 -mavx2

CC_OPT = $(INCLUDES) $(MINI_AUDIO_OPT_GCC) -Wall -Werror

LD_OPT = -lconfig

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	CC_OPT += $(MINI_AUDIO_OPT_LINUX)
endif

include Makefile.sources

.DEFAULT_GOAL = all

BUILD_TARGETS := $(patsubst %.c, $(BUILD_DIR)/%.o, $(SRC_FILES))
BUILD_DEPENDENCIES := $(patsubst %.c, $(BUILD_DIR)/%.d, $(SRC_FILES))

EXECUTABLES := $(patsubst %.c, %, $(wildcard ./*.c))

-include $(BUILD_DEPENDENCIES)

# Build all executables

all: $(EXECUTABLES)

# Build objects from the src files

$(BUILD_DIR):
	mkdir -p $@/src/logicbackend
	mkdir -p $@/src/audiobackend
	mkdir -p $@/src/server
	mkdir -p $@/src/intercom
	mkdir -p $@/lib

$(BUILD_DIR)/%.o: %.c
	$(CC) $(CC_OPT) -c -MMD -MP $(RELEASE) -o $@ $<

# Build executables

$(EXECUTABLES): $(BUILD_DIR) $(BUILD_TARGETS) 
	gcc -o $(BUILD_DIR)/$@ $(BUILD_TARGETS) $@.c $(CC_OPT) $(LD_OPT)
	@if [ -n "$(filter $@, $(MAKECMDGOALS))" ]; then \
	  $(BUILD_DIR)/$@ $(filter-out $@, $(MAKECMDGOALS)); \
	fi

# Misc

HEADER_FILES = $(shell find ./include -name "*.h")

clean:
	@rm -rf $(BUILD_DIR)

.PHONY: all format format-check test