#include "dxvm_mali_context.h"

namespace dxvk {

  DxvmMaliContext::DxvmMaliContext(bool isMali, const Config& config)
  : m_isMali(isMali)
  , m_config(config) {
  }

}
