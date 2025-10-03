# Простой Makefile для сборки тестов с учётом зависимостей модулей
CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic
CPPFLAGS ?= -I./stubs -I.. -I../libs -I../src
LDFLAGS ?=
LDLIBS ?= -lsodium

BUILD_DIR ?= build

SUPPORT_SRCS := ../src/libs_includes.cpp \
                ../src/message_buffer.cpp \
                ../src/tx_module.cpp \
                ../src/rx_module.cpp \
                ../src/radio_sx1262.cpp
SUPPORT_OBJS := $(patsubst ../%.cpp,$(BUILD_DIR)/support/%.o,$(SUPPORT_SRCS))
TEST_SRCS ?= test_key_transfer.cpp test_radio_irq_logging.cpp test_radio_send_error_recovery.cpp \
             test_http_image_upload.cpp test_sse_backpressure.cpp test_keytransfer_receive_async.cpp \
             test_key_safe_mode.cpp
TEST_BINS := $(patsubst %.cpp,$(BUILD_DIR)/%,$(TEST_SRCS))
TEST_TARGETS := $(patsubst %.cpp,%,$(TEST_SRCS))

.PHONY: all clean $(TEST_TARGETS)

all: $(TEST_BINS)

$(TEST_TARGETS): %: $(BUILD_DIR)/%
	@echo "Бинарник $< готов"

# Специализированный юнит-тест safe mode не требует тяжёлых зависимостей Radio/LoRa.
$(BUILD_DIR)/test_key_safe_mode: test_key_safe_mode.cpp test_key_safe_mode_support.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ ../src/key_safe_mode.cpp $(LDFLAGS) -o $@

# Общее правило для остальных тестов, которым нужны поддерживающие модули.
$(BUILD_DIR)/%: %.cpp $(SUPPORT_OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< $(SUPPORT_OBJS) $(LDFLAGS) $(LDLIBS) -o $@

# Тест ограничения SSE работает автономно и не требует поддержки Radio/LoRa
$(BUILD_DIR)/test_sse_backpressure: test_sse_backpressure.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(BUILD_DIR)/support/%.o: ../%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
