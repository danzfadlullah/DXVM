#include "Scope.h"

#include <algorithm>
#include <sstream>

namespace dxvmt {

  namespace {
    bool startsWith(const std::string& s, const char* prefix) {
      const size_t n = std::char_traits<char>::length(prefix);
      return s.size() >= n && !s.compare(0u, n, prefix);
    }
  }

  std::vector<Api> parseApis(const std::string& csv) {
    if (csv.empty() || csv == "all")
      return { Api::D3D11, Api::D3D10, Api::D3D9 };

    std::vector<Api> result;
    std::stringstream stream(csv);
    std::string token;

    while (std::getline(stream, token, ',')) {
      const auto first = token.find_first_not_of(" \t");
      if (first == std::string::npos)
        continue;
      const auto last = token.find_last_not_of(" \t");
      token = token.substr(first, last - first + 1u);

      if (token == "d3d11")      result.push_back(Api::D3D11);
      else if (token == "d3d10") result.push_back(Api::D3D10);
      else if (token == "d3d9")  result.push_back(Api::D3D9);
    }

    if (result.empty())
      return { Api::D3D11, Api::D3D10, Api::D3D9 };

    return result;
  }

  Backend parseBackend(const std::string& name) {
    if (name == "wrapper") return Backend::Wrapper;
    if (name == "forced")  return Backend::WrapperForced;
    return Backend::Raw;
  }

  Api apiOfScene(const std::string& sceneName) {
    if (startsWith(sceneName, "d3d9."))  return Api::D3D9;
    if (startsWith(sceneName, "d3d10.")) return Api::D3D10;
    return Api::D3D11;
  }

  std::string apiSectionHeader(Api api) {
    switch (api) {
      case Api::D3D11: return "== Direct3D 11 suite ==";
      case Api::D3D10: return "== Direct3D 10 suite ==";
      case Api::D3D9:  return "== Direct3D 9 suite ==";
    }
    return "== Direct3D 11 suite ==";
  }

}
