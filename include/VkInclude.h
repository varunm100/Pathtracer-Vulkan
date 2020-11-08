#pragma once

#define VK_ENABLE_BETA_EXTENSIONS
#include <volk/volk.h>
#include <assert.h>

#define VK_CHECK(call)							\
  do {									\
    if (call != VK_SUCCESS) {						\
      err_log("vk error {}: File '{}', line {}", call,__FILE__, __LINE__); \
      assert(0);							\
    }									\
  } while (0)								\
