# Простой Makefile для сборки тестов с учётом зависимостей модулей
CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic
CPPFLAGS ?= -I.. -I../libs
LDFLAGS ?=
LDLIBS ?= -lsodium

BUILD_DIR ?= build

SUPPORT_SRCS := ../libs_includes.cpp \
                ../logger.cpp \
                ../message_buffer.cpp \
                ../tx_module.cpp \
                ../rx_module.cpp
SUPPORT_OBJS := $(patsubst ../%.cpp,$(BUILD_DIR)/support/%.o,$(SUPPORT_SRCS))
TEST_SRCS ?= test_key_transfer.cpp test_key_exchange_compat.cpp
TEST_BINS := $(patsubst %.cpp,$(BUILD_DIR)/%,$(TEST_SRCS))
TEST_TARGETS := $(patsubst %.cpp,%,$(TEST_SRCS))

.PHONY: all clean $(TEST_TARGETS)

all: $(TEST_BINS)

$(TEST_TARGETS): %: $(BUILD_DIR)/%
	@echo "Бинарник $< готов"

$(BUILD_DIR)/%: %.cpp $(SUPPORT_OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< $(SUPPORT_OBJS) $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD_DIR)/support/%.o: ../%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
