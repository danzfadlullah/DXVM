#include "Report.h"

#include <cstdio>

namespace dxvmt {

  namespace {

    const char* statusKind(SceneStatus status) {
      switch (status) {
        case SceneStatus::Pass:  return "PASS";
        case SceneStatus::Fail:  return "FAIL";
        case SceneStatus::Skip:  return "SKIP";
        case SceneStatus::Crash: return "CRASH";
      }
      return "FAIL";
    }

    // DXML aligns the detail at column 15 but never emits zero spaces.
    constexpr size_t kNameColumn = 15u;

  }

  std::string formatResult(const std::string& name, const SceneResult& result) {
    const size_t pad = name.size() >= kNameColumn
      ? 1u
      : kNameColumn - name.size();

    std::string line = statusKind(result.status);
    line += ' ';
    line += name;
    line.append(pad, ' ');
    line += result.detail;
    return line;
  }

  std::string formatSummary(int passed, int failed) {
    return "SUMMARY " + std::to_string(passed) + " passed, "
         + std::to_string(failed) + " failed";
  }

  std::string formatHeader(
          const std::string& timestamp,
          const std::string& harnessVersion,
          int                code,
          int                passed,
          int                failed) {
    std::string out;
    out += "DXVM Test Pack report, " + timestamp + "\n";
    out += "Harness: " + harnessVersion + " (code " + std::to_string(code) + ")\n";
    out += "Total: " + std::to_string(passed) + " passed, "
         + std::to_string(failed) + " failed or crashed\n";
    out += "\n";
    return out;
  }

  std::string formatTotals(const std::vector<TotalsRow>& rows) {
    char buf[256];
    std::string out = "== Totals (pass/total) ==\n";

    for (const auto& r : rows) {
      const std::string c0 = std::to_string(r.passed0) + "/" + std::to_string(r.total0);
      const std::string c1 = std::to_string(r.passed1) + "/" + std::to_string(r.total1);
      const std::string c2 = std::to_string(r.passed2) + "/" + std::to_string(r.total2);

      std::snprintf(buf, sizeof(buf), "%-75s%-11s%-11s%-11s",
        r.label.c_str(), c0.c_str(), c1.c_str(), c2.c_str());
      out += buf;
      out += '\n';
    }

    return out;
  }

}