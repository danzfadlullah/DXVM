#include "Child.h"
#include "Device.h"
#include "Registry.h"
#include "Report.h"
#include "SelfTestScenes.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

namespace dxvmt {

  // Scene group registrars. Each group lives in its own translation unit and is
  // a no-op when the host cannot build Direct3D code.
  void registerDeviceScenes(Registry& registry);
  void registerMsaaScenes(Registry& registry);
  void registerStateScenes(Registry& registry);
  void registerTextureScenes(Registry& registry);

}

namespace {

  constexpr const char* kHarnessVersion = "0.1.0";
  constexpr int         kHarnessCode    = 1;

  void usage() {
    std::printf(
      "dxvm-harness — DXVM Direct3D test harness\n"
      "  --list                 list registered scenes\n"
      "  --scenes a,b           run only these scenes\n"
      "  --self-test            run the built-in stub scenes\n"
      "  --env-file PATH        read KEY=VALUE lines before running\n"
      "  --set KEY=VALUE        set one variable (repeatable)\n"
      "  --out PATH             also write the report to PATH\n"
      "  --selftest-child NAME  internal: run one scene in this process\n");
  }

  bool isSelfTestChild(int argc, char** argv, std::string& sceneName) {
    for (int i = 1; i + 1 < argc; i++) {
      if (!std::strcmp(argv[i], "--selftest-child")) {
        sceneName = argv[i + 1];
        return true;
      }
    }
    return false;
  }

  /** Read the first KEY=VALUE line from a file, or empty on failure. */
  std::string readWholeFile(const std::string& path) {
    std::string text;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f)
      return text;
    char buf[512];
    size_t n;
    while ((n = std::fread(buf, 1u, sizeof(buf), f)) > 0u)
      text.append(buf, n);
    std::fclose(f);
    return text;
  }

  std::string timestampNow() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm);
    return buf;
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
  registerSelfTestScenes(registry);
  registerDeviceScenes(registry);
  registerMsaaScenes(registry);
  registerStateScenes(registry);
  registerTextureScenes(registry);

  // Child mode must run before anything else: it exists to be isolated.
  std::string childScene;
  if (isSelfTestChild(argc, argv, childScene)) {
    const auto sel = registry.select(childScene);
    if (sel.empty()) {
      std::fprintf(stderr, "unknown scene: %s\n", childScene.c_str());
      return 2;
    }
    SceneResult r = sel[0].fn();
    std::printf("%s\n", formatResult(sel[0].name, r).c_str());
    std::fflush(stdout);
    return r.status == SceneStatus::Pass ? 0 : 1;
  }

  if (hasFlag(argc, argv, "--list")) {
    for (const auto& entry : registry.entries())
      std::printf("%s\n", entry.name.c_str());
    return 0;
  }

  const std::string scenesArg = argValue(argc, argv, "--scenes", "");
  const std::string outPath   = argValue(argc, argv, "--out", "");
  const std::string envFile   = argValue(argc, argv, "--env-file", "");

  // Environment is applied before any device creation.
  if (!envFile.empty())
    applyEnv(parseEnvLines(readWholeFile(envFile)));

  for (int i = 1; i + 1 < argc; i++) {
    if (!std::strcmp(argv[i], "--set"))
      applyEnv(parseEnvLines(std::string(argv[i + 1]) + "\n"));
  }

  const std::string effectiveScenes = hasFlag(argc, argv, "--self-test")
    ? std::string("selftest.pass,selftest.fail,selftest.skip,selftest.crash")
    : scenesArg;

  const auto selected = registry.select(effectiveScenes);

  struct Row { std::string name; SceneResult result; };
  std::vector<Row> rows;
  rows.reserve(selected.size());

  int passed = 0, failed = 0;

  for (const auto& entry : selected) {
    ChildOutcome outcome = runSceneAsChild(argv[0], entry.name);
    const SceneResult& result = outcome.result;

    if (result.status == SceneStatus::Pass)
      passed++;
    else if (result.status != SceneStatus::Skip)
      failed++;

    rows.push_back(Row{ entry.name, result });
  }

  // The header carries the final totals, so it is emitted after the run.
  std::string report = formatHeader(timestampNow(), kHarnessVersion,
    kHarnessCode, passed, failed);

#ifdef _WIN32
  report += "GPU: " + describeGpu() + "\n";
  report += "\n";
#endif

  for (const auto& row : rows) {
    report += formatResult(row.name, row.result);
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