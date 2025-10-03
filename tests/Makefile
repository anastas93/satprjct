# Простой Makefile для сборки тестов с учётом зависимостей модулей
CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic
CPPFLAGS ?= -I.. -I../libs -I./stubs -I../src
LDFLAGS ?=
LDLIBS ?= -lsodium

BUILD_DIR ?= build

SUPPORT_SRCS := ../src/libs_includes.cpp \
                ../src/message_buffer.cpp \
                ../src/tx_module.cpp \
                ../src/rx_module.cpp \
                ../src/radio_sx1262.cpp
SUPPORT_OBJS := $(patsubst ../%.cpp,$(BUILD_DIR)/support/%.o,$(SUPPORT_SRCS))
TEST_SRCS ?= test_key_transfer.cpp test_radio_irq_logging.cpp
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
