#pragma once

#include "Scene.h"

#include <string>
#include <utility>
#include <vector>

namespace dxvmt {

  /**
   * \brief What came back from a child process run
   *
   * \c produced is true when the child printed a result line. A child that
   * died without one is reported as \c Crash carrying its exit code, so every
   * scene always yields exactly one line in the report.
   */
  struct ChildOutcome {
    bool        produced = false;
    SceneResult result   = SceneResult::skip("no result");
    int         exitCode = -1;
  };

  /** \brief Run one scene in a child process of \c exePath */
  ChildOutcome runSceneAsChild(const std::string& exePath, const std::string& sceneName);

  /** \brief Parse `KEY=VALUE` lines, skipping blanks and `#` comments */
  std::vector<std::pair<std::string, std::string>> parseEnvLines(const std::string& text);

  /** \brief Apply parsed pairs to this process environment */
  void applyEnv(const std::vector<std::pair<std::string, std::string>>& pairs);

}