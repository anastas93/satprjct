#pragma once
#ifndef LIBS_QOS_H
#define LIBS_QOS_H

#include <stdint.h>

// Перечисления уровней приоритета QoS и режимов планировщика
enum class Qos : uint8_t { High=0, Normal=1, Low=2 };
enum class QosMode : uint8_t { Strict=0, Weighted421=1 };

#endif // LIBS_QOS_H
