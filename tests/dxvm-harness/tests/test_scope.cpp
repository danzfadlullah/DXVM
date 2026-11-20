#include "../src/Scope.h"

#include <cstdio>
#include <string>

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (false)

int main() {
  using namespace dxvmt;

  // API selection
  CHECK(parseApis("all").size() == 3);
  CHECK(parseApis("d3d11").size() == 1);
  CHECK(parseApis("d3d11")[0] == Api::D3D11);
  CHECK(parseApis("d3d9,d3d10").size() == 2);
  CHECK(parseApis("").size() == 3);           // default: everything
  CHECK(parseApis("bogus").size() == 3);      // unknown falls back to all

  // Backend selection
  CHECK(parseBackend("raw")    == Backend::Raw);
  CHECK(parseBackend("wrapper")== Backend::Wrapper);
  CHECK(parseBackend("forced") == Backend::WrapperForced);
  CHECK(parseBackend("")       == Backend::Raw);
  CHECK(parseBackend("bogus")  == Backend::Raw);

  // A scene belongs to exactly one API, decided by its name prefix.
  CHECK(apiOfScene("d3d9.device")  == Api::D3D9);
  CHECK(apiOfScene("d3d9.msaa")    == Api::D3D9);
  CHECK(apiOfScene("d3d10.create") == Api::D3D10);
  CHECK(apiOfScene("msaa.coverage")== Api::D3D11);
  CHECK(apiOfScene("bcn.bc1")      == Api::D3D11);

  // Section headers and their order in the report.
  CHECK(apiSectionHeader(Api::D3D11) == "== Direct3D 11 suite ==");
  CHECK(apiSectionHeader(Api::D3D10) == "== Direct3D 10 suite ==");
  CHECK(apiSectionHeader(Api::D3D9)  == "== Direct3D 9 suite ==");

  if (failures) { std::fprintf(stderr, "%d check(s) failed\n", failures); return 1; }
  std::printf("test_scope: OK\n");
  return 0;
}
