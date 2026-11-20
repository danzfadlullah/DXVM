#include "Registry.h"
#include "Report.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

  constexpr const char* kHarnessVersion = "0.1.0";
  constexpr int         kHarnessCode    = 1;

  void usage() {
    std::printf(
      "dxvm-harness — DXVM Direct3D test harness\n"
      "  --list                 list registered scenes\n"
      "  --scenes a,b           run only these scenes\n"
      "  --self-test            run the built-in stub scenes\n"
      "  --out PATH             also write the report to PATH\n");
  }

  std::string argValue(int argc, char** argv, const char* flag, const char* fallback) {
    for (int i = 1; i + 1 < argc; i++) {
      if (!std::strcmp(argv[i], flag))
        return argv[i + 1];
    }
    return fallback;
  }

  bool hasFlag(int argc, char** argv, const char* flag) {
    for (int i = 1; i < argc; i++) {
      if (!std::strcmp(argv[i], flag))
        return true;
    }
    return false;
  }

}

int main(int argc, char** argv) {
  using namespace dxvmt;

  if (hasFlag(argc, argv, "--help") || hasFlag(argc, argv, "-h")) {
    usage();
    return 0;
  }

  Registry registry;
  // Task 1 registers no scenes; later tasks add their groups here.

  if (hasFlag(argc, argv, "--list")) {
    for (const auto& entry : registry.entries())
      std::printf("%s\n", entry.name.c_str());
    return 0;
  }

  const std::string scenesArg = argValue(argc, argv, "--scenes", "");
  const std::string outPath   = argValue(argc, argv, "--out", "");

  const auto selected = registry.select(scenesArg);

  std::string report = formatHeader("2026-11-21 00:00", kHarnessVersion,
    kHarnessCode, 0, 0);

  int passed = 0, failed = 0;

  for (const auto& entry : selected) {
    SceneResult result = entry.fn();

    if (result.status == SceneStatus::Pass)
      passed++;
    else if (result.status != SceneStatus::Skip)
      failed++;

    report += formatResult(entry.name, result);
    report += '\n';
  }

  report += formatSummary(passed, failed);
  report += '\n';

  std::fputs(report.c_str(), stdout);

  if (!outPath.empty()) {
    if (FILE* f = std::fopen(outPath.c_str(), "wb")) {
      std::fwrite(report.data(), 1u, report.size(), f);
      std::fclose(f);
    }
  }

  return failed == 0 ? 0 : 1;
}