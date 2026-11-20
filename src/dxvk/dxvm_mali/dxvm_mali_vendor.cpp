#include "dxvm_mali_vendor.h"

namespace dxvk {

  bool dxvmIsClassicVendor(uint32_t vendorId) {
    switch (vendorId) {
      case 0x1002u: // AMD
      case 0x10deu: // NVIDIA
      case 0x8086u: // Intel
      case 0x13b5u: // ARM Mali
        return true;
      default:
        return false;
    }
  }

}
