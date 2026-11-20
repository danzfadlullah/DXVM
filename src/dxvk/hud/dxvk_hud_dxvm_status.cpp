#include "dxvk_hud_dxvm_status.h"

namespace dxvk::hud {

  std::string dxvmHudStatusLine(bool useCpuBcn, bool dynRender, bool sync2) {
    std::string s = "BCn→ASTC:";
    s += useCpuBcn ? "CPU" : "GPU";
    s += " · DynR:";
    s += dynRender ? "ON" : "OFF";
    s += " · Sync2:";
    s += sync2 ? "ON" : "OFF";
    return s;
  }

}
