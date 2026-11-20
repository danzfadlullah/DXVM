#pragma once

#include "dxvm_mali_config.h"

namespace dxvk {

  /**
   * \brief Per-device holder for Mali state
   *
   * Owns the detection flag and the parsed Mali configuration. Later tasks
   * add owned sub-objects and the \c dxvmMali* free-function API operates on
   * this context.
   */
  class DxvmMaliContext {
  public:
    DxvmMaliContext(bool isMali, const Config& config);

    bool isMali() const {
      return m_isMali;
    }

    const DxvmMaliConfig& config() const {
      return m_config;
    }

  private:
    bool m_isMali;
    DxvmMaliConfig m_config;
  };

}
