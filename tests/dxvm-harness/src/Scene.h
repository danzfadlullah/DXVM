#pragma once

#include <string>

namespace dxvmt {

  /**
   * \brief Outcome kind of a single scene
   */
  enum class SceneStatus {
    Pass,
    Fail,
    Skip,
    Crash,
  };

  /**
   * \brief Result a scene returns
   *
   * \c detail carries the measured values against the expectations, in the
   * wording DXML uses, so reports stay comparable.
   */
  struct SceneResult {
    SceneStatus status = SceneStatus::Skip;
    std::string detail;

    static SceneResult pass(std::string detail) {
      return SceneResult{ SceneStatus::Pass, std::move(detail) };
    }

    static SceneResult fail(std::string detail) {
      return SceneResult{ SceneStatus::Fail, std::move(detail) };
    }

    static SceneResult skip(std::string detail) {
      return SceneResult{ SceneStatus::Skip, std::move(detail) };
    }

    static SceneResult crash(std::string detail) {
      return SceneResult{ SceneStatus::Crash, std::move(detail) };
    }
  };

  /**
   * \brief Scene entry point
   *
   * Each scene creates the Direct3D device it needs: it always runs inside its
   * own child process, so no device state crosses the process boundary and a
   * crashed device cannot poison a later scene.
   */
  using SceneFn = SceneResult (*)();

}