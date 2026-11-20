#include "../src/Report.h"
#include "../src/Registry.h"

#include <cstdio>
#include <string>

static int failures = 0;

#define CHECK(cond) do {                                                  \
  if (!(cond)) {                                                          \
    std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
    ++failures;                                                           \
  }                                                                       \
} while (false)

int main() {
  using namespace dxvmt;

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

  if (failures) {
    std::fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
  }

  std::printf("test_report: OK\n");
  return 0;
}