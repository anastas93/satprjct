# Простой Makefile для сборки тестов с учётом криптографических зависимостей
CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic
CPPFLAGS ?= -I.. -I../libs
LDFLAGS ?=
LDLIBS ?= -lsodium

BUILD_DIR ?= build

CRYPTO_SRCS := $(wildcard ../libs/crypto/*.cpp)
CRYPTO_OBJS := $(patsubst ../libs/%.cpp,$(BUILD_DIR)/libs/%.o,$(CRYPTO_SRCS))
TEST_SRCS ?= test_key_transfer.cpp
TEST_BINS := $(patsubst %.cpp,$(BUILD_DIR)/%,$(TEST_SRCS))
TEST_TARGETS := $(patsubst %.cpp,%,$(TEST_SRCS))

.PHONY: all clean $(TEST_TARGETS)

all: $(TEST_BINS)

$(TEST_TARGETS): %: $(BUILD_DIR)/%
	@echo "Бинарник $< готов"

$(BUILD_DIR)/%: %.cpp $(CRYPTO_OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< $(CRYPTO_OBJS) $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD_DIR)/libs/%.o: ../libs/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
