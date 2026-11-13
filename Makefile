TARGET   := nodle
BUILD    := build
SRC_DIR  := src
TEST_DIR := tests
PREFIX   ?= /usr/local

CXX      ?= g++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS += -Iinclude -MMD -MP
LDFLAGS  ?=

ifdef DEBUG
CXXFLAGS := -std=c++20 -O0 -g -Wall -Wextra -Wpedantic -fsanitize=address,undefined
LDFLAGS  += -fsanitize=address,undefined
endif

# Library sources = everything in src/ except main.cpp
SRCS      := $(shell find $(SRC_DIR) -name '*.cpp' | sort)
LIB_SRCS  := $(filter-out $(SRC_DIR)/main.cpp,$(SRCS))
LIB_OBJS  := $(LIB_SRCS:%.cpp=$(BUILD)/%.o)
MAIN_OBJ  := $(BUILD)/$(SRC_DIR)/main.o

# GoogleTest: prefer pkg-config, fall back to plain -l flags.
GTEST_CFLAGS := $(shell pkg-config --cflags gtest 2>/dev/null)
GTEST_LIBS   := $(or $(shell pkg-config --libs gtest_main 2>/dev/null),-lgtest_main -lgtest -pthread)

# All tests/*.cpp are compiled into a single gtest runner.
TEST_SRCS := $(shell find $(TEST_DIR) -name '*.cpp' | sort)
TEST_OBJS := $(TEST_SRCS:%.cpp=$(BUILD)/%.o)
TEST_BIN  := $(BUILD)/$(TEST_DIR)/run_tests

.PHONY: all test run install clean format
ALL_OBJS := $(MAIN_OBJ) $(LIB_OBJS) $(TEST_OBJS)

all: $(BUILD)/$(TARGET) $(TEST_BIN)

$(BUILD)/$(TARGET): $(MAIN_OBJ) $(LIB_OBJS)
	@mkdir -p $(@D)
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
	@printf '{"directory":"%s","file":"%s","command":"%s"},\n' '$(CURDIR)' '$<' '$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $<' > $@.json

$(BUILD)/$(TEST_DIR)/%.o: CPPFLAGS += $(GTEST_CFLAGS)

$(TEST_BIN): $(TEST_OBJS) $(LIB_OBJS)
	@mkdir -p $(@D)
	$(CXX) $^ $(LDFLAGS) $(GTEST_LIBS) -o $@

test: $(TEST_BIN)
	$(TEST_BIN)

run: all
	$(BUILD)/$(TARGET)

install: all
	install -Dm755 $(BUILD)/$(TARGET) $(DESTDIR)$(PREFIX)/bin/$(TARGET)

format:
	clang-format -i $(shell find $(SRC_DIR) $(TEST_DIR) include -name '*.cpp' -o -name '*.hpp')

clean:
	rm -rf $(BUILD) compile_commands.json

-include $(shell find $(BUILD) -name '*.d' 2>/dev/null)