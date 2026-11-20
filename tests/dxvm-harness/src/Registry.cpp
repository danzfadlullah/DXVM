#include "Registry.h"

#include <algorithm>
#include <sstream>

namespace dxvmt {

  void Registry::add(const std::string& name, SceneFn fn) {
    // A duplicate name would print twice in the report and double-count in the
    // summary, so the first registration wins and the duplicate is refused.
    for (const auto& e : m_entries) {
      if (e.name == name)
        return;
    }

    m_entries.push_back(Entry{ name, fn });
  }

  std::vector<Registry::Entry> Registry::select(const std::string& csv) const {
    if (csv.empty())
      return m_entries;

    std::vector<std::string> wanted;
    std::stringstream stream(csv);
    std::string token;

    while (std::getline(stream, token, ',')) {
      // Tolerate surrounding whitespace: "--scenes a, b" is common.
      const auto first = token.find_first_not_of(" \t");
      if (first == std::string::npos)
        continue;
      const auto last = token.find_last_not_of(" \t");
      wanted.push_back(token.substr(first, last - first + 1u));
    }

    std::vector<Entry> result;

    for (const auto& entry : m_entries) {
      if (std::find(wanted.begin(), wanted.end(), entry.name) != wanted.end())
        result.push_back(entry);
    }

    return result;
  }

}