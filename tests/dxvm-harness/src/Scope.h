#pragma once

#include <string>
#include <vector>

namespace dxvmt {

  enum class Api { D3D11, D3D10, D3D9 };

  enum class Backend { Raw, Wrapper, WrapperForced };

  /** \brief Parse `--api` (comma list or "all"); unknown input means all */
  std::vector<Api> parseApis(const std::string& csv);

  /** \brief Parse `--backend`; unknown input means raw */
  Backend parseBackend(const std::string& name);

  /** \brief Which API a scene belongs to, from its name prefix */
  Api apiOfScene(const std::string& sceneName);

  /** \brief The `== ... ==` header for an API section */
  std::string apiSectionHeader(Api api);

}
