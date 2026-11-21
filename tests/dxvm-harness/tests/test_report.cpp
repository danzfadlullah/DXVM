#include "../src/Report.h"
#include "../src/Registry.h"
#include "../src/Child.h"
#include "../src/SelfTestScenes.h"

#include <cstdio>
#include <string>

static int failures = 0;

#define CHECK(cond) do {                                                  \
  if (!(cond)) {                                                          \
    std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
    ++failures;                                                           \
  }                                                                       \
} while (false)

int main(int argc, char** argv) {
  using namespace dxvmt;

  // Child mode: run one self-test scene in-process and print its result line.
  if (argc >= 3 && std::string(argv[1]) == "--selftest-child") {
    for (int i = 1; i + 1 < argc; i++) {
      if (std::string(argv[i]) == "--child-out")
        std::freopen(argv[i + 1], "wb", stdout);
    }
    Registry reg;
    registerSelfTestScenes(reg);
    const auto sel = reg.select(argv[2]);
    if (sel.empty()) {
      std::fprintf(stderr, "unknown scene: %s\n", argv[2]);
      return 2;
    }
    SceneResult r = sel[0].fn();
    std::printf("%s\n", formatResult(sel[0].name, r).c_str());
    std::fflush(stdout);
    return 0;
  }

  // --- Result lines: name padded to column 15, minimum one space. ---
  CHECK(formatResult("depth", SceneResult::pass("left half red")) ==
        "PASS depth          left half red");
  CHECK(formatResult("msaa4.rgba8", SceneResult::pass("64 partial pixels")) ==
        "PASS msaa4.rgba8    64 partial pixels");
  CHECK(formatResult("msaa16.sampleindex", SceneResult::pass("red is 128, expected 128")) ==
        "PASS msaa16.sampleindex red is 128, expected 128");
  CHECK(formatResult("x", SceneResult::fail("red is 26, expected 9")) ==
        "FAIL x              red is 26, expected 9");
  CHECK(formatResult("x", SceneResult::skip("not ported yet")) ==
        "SKIP x              not ported yet");

  // --- Section summary ---
  CHECK(formatSummary(90, 2) == "SUMMARY 90 passed, 2 failed");

  // --- Header block ---
  std::string h = formatHeader("2026-11-21 04:53", "0.1.0", 1, 314, 0);
  CHECK(h.rfind("DXVM Test Pack report, 2026-11-21 04:53", 0) == 0);
  CHECK(h.find("Total: 314 passed, 0 failed or crashed") != std::string::npos);
  CHECK(h.find("Harness: 0.1.0 (code 1)") != std::string::npos);

  // --- Totals block ---
  std::string t = formatTotals({ TotalsRow{"D3D11", 12, 17, 12, 17, 12, 17} });
  CHECK(t.rfind("== Totals (pass/total) ==", 0) == 0);
  CHECK(t.find("D3D11") != std::string::npos);
  CHECK(t.find("12/17") != std::string::npos);

  // --- Registry selection ---
  Registry reg;
  reg.add("alpha", [] { return SceneResult::pass("a"); });
  reg.add("beta",  [] { return SceneResult::pass("b"); });
  CHECK(reg.entries().size() == 2);
  CHECK(reg.select("").size() == 2);
  CHECK(reg.select("beta").size() == 1);
  CHECK(reg.select("beta")[0].name == "beta");
  CHECK(reg.select("nope").empty());
  CHECK(reg.select("beta,alpha")[0].name == "alpha"); // registry order preserved

  // A duplicate registration must not produce a second report line.
  reg.add("alpha", [] { return SceneResult::fail("dup"); });
  CHECK(reg.entries().size() == 2);

  // --- Env file parsing ---
  auto p = parseEnvLines("WRAPPER_LOG_LEVEL=info\r\n# comment\n\nDXVK_HUD=dxvm\n");
  CHECK(p.size() == 2);
  CHECK(p[0].first == "WRAPPER_LOG_LEVEL" && p[0].second == "info");
  CHECK(p[1].first == "DXVK_HUD" && p[1].second == "dxvm");
  CHECK(parseEnvLines("no_equals_sign\n").empty());

  // --- Crash containment: every scene yields exactly one result line ---
  const std::string self = argv[0];
  ChildOutcome pass = runSceneAsChild(self, "selftest.pass");
  CHECK(pass.produced);
  CHECK(pass.result.status == SceneStatus::Pass);

  ChildOutcome fail = runSceneAsChild(self, "selftest.fail");
  CHECK(fail.produced);
  CHECK(fail.result.status == SceneStatus::Fail);

  ChildOutcome crash = runSceneAsChild(self, "selftest.crash");
  CHECK(crash.result.status == SceneStatus::Crash);
  CHECK(!crash.result.detail.empty());

  // CRASH details carry the spec's leading colon before the reason.
  CHECK(crash.result.detail[0] == ':');

  if (failures) {
    std::fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
  }

  std::printf("test_report: OK\n");
  return 0;
}