#include <cassert>
#include <string>
#include <vector>

#include "libs/key_transfer_waiter/key_transfer_waiter.h"
#include "libs/simple_logger/simple_logger.h"

// Регрессионный тест: убеждаемся, что автомат ожидания KEYTRANSFER не блокирует
// доставку логов SSE и корректно уведомляет HTTP/Serial потребителей.
int main() {
  KeyTransferWaiter waiter;

  // HTTP-клиент запускает ожидание и получает логи в процессе.
  waiter.start(/*now_ms=*/0, /*timeout_ms=*/50, /*serial_watcher=*/false, /*http_watcher=*/true);
  assert(waiter.isWaiting());
  SimpleLogger::logStatus("LOG1");
  auto logs = SimpleLogger::getLast(1);
  assert(!logs.empty());
  assert(logs.back() == "LOG1");
  assert(!waiter.isExpired(40));
  assert(waiter.isExpired(60));

  // После тайм-аута HTTP должен получить JSON с ошибкой.
  waiter.finalizeError("{\"error\":\"timeout\"}");
  assert(!waiter.isWaiting());
  assert(waiter.httpPending());
  std::string http_result = waiter.consumeHttp();
  assert(!http_result.empty());
  assert(http_result.find("\"error\":\"timeout\"") != std::string::npos);
  assert(!waiter.httpPending());

  // Serial может запустить новый цикл ожидания и получить готовый результат.
  waiter.start(/*now_ms=*/100, /*timeout_ms=*/0, /*serial_watcher=*/true, /*http_watcher=*/false);
  waiter.finalizeSuccess("{\"status\":\"ok\"}");
  assert(!waiter.isWaiting());
  assert(waiter.serialPending());
  std::string serial_result = waiter.consumeSerial();
  assert(serial_result == "{\"status\":\"ok\"}");
  assert(!waiter.serialPending());

  // После выдачи результата автомат готов к следующему запуску и логи продолжают накапливаться.
  waiter.start(/*now_ms=*/200, /*timeout_ms=*/20, /*serial_watcher=*/false, /*http_watcher=*/true);
  SimpleLogger::logStatus("LOG2");
  auto logs2 = SimpleLogger::getLast(1);
  assert(!logs2.empty());
  assert(logs2.back() == "LOG2");
  waiter.finalizeSuccess("{\"status\":\"done\"}");
  assert(waiter.httpPending());
  assert(waiter.consumeHttp() == "{\"status\":\"done\"}");
  return 0;
}
