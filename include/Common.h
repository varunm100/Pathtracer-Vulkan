#pragma once
#include <assert.h>
#include <stdint.h>
#include <spdlog/spdlog.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

template <typename T, size_t Size>
char (*count_of_helper(T (&_arr)[Size]))[Size];

#define COUNT_OF(arr) (sizeof(*count_of_helper(arr)) + 0)

#define info_log(msg, ...) spdlog::info(msg, ##__VA_ARGS__);
#define err_log(msg, ...) spdlog::error(msg, ##__VA_ARGS__);
#define warn_log(msg, ...) spdlog::warn(msg, ##__VA_ARGS__);

#define assert_log(condition, msg) \
  do {				   \
    if (!(condition)) {		   \
      err_log(msg);		   \
      assert(0);		   \
    }				   \
  } while (0)			   \
