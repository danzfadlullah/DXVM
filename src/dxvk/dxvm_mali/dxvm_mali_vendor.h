#pragma once

#include <cstdint>

namespace dxvk {

  /** True for vendor IDs DXVK knows and handles natively (incl. ARM Mali). */
  bool dxvmIsClassicVendor(uint32_t vendorId);

}
