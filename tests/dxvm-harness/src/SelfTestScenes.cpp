#include "SelfTestScenes.h"

namespace dxvmt {

  namespace {

    SceneResult selfTestPass() {
      return SceneResult::pass("stub scene ran");
    }

    SceneResult selfTestFail() {
      return SceneResult::fail("stub scene failed on purpose");
    }

    SceneResult selfTestSkip() {
      return SceneResult::skip("stub scene skipped");
    }

    SceneResult selfTestCrash() {
      // Deliberate null dereference: proves a scene that kills its process
      // still yields a CRASH line instead of truncating the report.
      *static_cast<volatile int*>(nullptr) = 1;
      return SceneResult::pass("unreachable");
    }

  }

  void registerSelfTestScenes(Registry& registry) {
    registry.add("selftest.pass",  &selfTestPass);
    registry.add("selftest.fail",  &selfTestFail);
    registry.add("selftest.skip",  &selfTestSkip);
    registry.add("selftest.crash", &selfTestCrash);
  }

}