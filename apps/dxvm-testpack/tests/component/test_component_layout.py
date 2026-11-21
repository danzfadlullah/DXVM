"""Layout contract for a PanPlay component archive.

Read from PanPlay's own unpacker (Contents.kt / Containers.kt): the archive is
tar.xz or tar.zst with profile.json, system32/ and syswow64/ at the root;
profile.json carries `type` and `versionName`; PanPlay's DXVK slot is type DXVK;
files[] targets use ${system32}/ and ${syswow64}/.
"""
import os
import pathlib
import subprocess
import sys

import pytest

HERE = pathlib.Path(__file__).resolve().parents[2]
VERIFY = HERE / "tools" / "verify-component.py"
MAKE = HERE / "tools" / "make-fixture-wcp.py"


def run_verify(path):
    return subprocess.run([sys.executable, str(VERIFY), str(path)],
                          capture_output=True, text=True)


def fixture(tmp_path, name, *args):
    out = tmp_path / f"{name}.wcp"
    r = subprocess.run([sys.executable, str(MAKE), "--out", str(out), *args],
                       capture_output=True, text=True)
    assert r.returncode == 0, r.stdout + r.stderr
    return out


def test_rejects_wrong_type(tmp_path):
    p = fixture(tmp_path, "wrongtype", "--type", "VKD3D")
    r = run_verify(p)
    assert r.returncode != 0
    assert "type" in (r.stdout + r.stderr).lower()


def test_rejects_missing_version_name(tmp_path):
    p = fixture(tmp_path, "nover", "--version-name", "")
    r = run_verify(p)
    assert r.returncode != 0


def test_rejects_system32_that_is_not_arm64ec(tmp_path):
    # A component with x86_64 in system32 is exactly the mistake this guards:
    # PanPlay prefixes are arm64ec, so system32 must hold arm64ec DLLs.
    p = fixture(tmp_path, "wrongarch", "--system32-arch", "x86_64")
    r = run_verify(p)
    assert r.returncode != 0
    assert "arm64ec" in (r.stdout + r.stderr).lower()


def test_rejects_missing_marker(tmp_path):
    p = fixture(tmp_path, "nomarker", "--no-marker")
    r = run_verify(p)
    assert r.returncode != 0
    assert "marker" in (r.stdout + r.stderr).lower()


def test_accepts_a_conforming_component(tmp_path):
    p = fixture(tmp_path, "good")
    r = run_verify(p)
    assert r.returncode == 0, r.stdout + r.stderr


def _real_component() -> pathlib.Path | None:
    """The first built DXVM component visible from this checkout, or None.

    A built component lives outside the repository (it is 12 MB and per-run), so it is looked
    for in the usual places rather than at one hard-coded path: `$DXVM_WCP`, the caller's tmp
    dir, the repo's parent, and the build script's default output. Paths that cannot even be
    stat'd (a CI runner has no `/root`) are treated as absent, not as a failure.
    """
    candidates: list[pathlib.Path] = []
    if env := os.environ.get("DXVM_WCP"):
        candidates.append(pathlib.Path(env))
    for base in (pathlib.Path(os.environ.get("TMPDIR", "/tmp")),
                 pathlib.Path(os.environ.get("RUNNER_TEMP", "/tmp")),
                 HERE.parents[2].parent,
                 pathlib.Path.home() / "workspace"):
        try:
            candidates.extend(sorted(base.glob("dxvm-*.wcp")))
        except (OSError, PermissionError):
            continue
    for c in candidates:
        try:
            if c.is_file():
                return c
        except (OSError, PermissionError):
            continue
    return None


def test_accepts_the_real_component_if_built():
    real = _real_component()
    if real is None:
        pytest.skip("DXVM component not built in this environment (set DXVM_WCP to check one)")
    r = run_verify(real)
    assert r.returncode == 0, f"{real}\n" + r.stdout + r.stderr

