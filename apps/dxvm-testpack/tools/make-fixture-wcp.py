#!/usr/bin/env python3
"""Build a component archive fixture for tests/component.

The DLLs are minimal PE stubs: only the machine type and (for ARM64EC) the
`arm64x` marker matter, because that is what PanPlay's arm64ec prefix requires.
"""
import argparse
import io
import json
import struct
import subprocess
import tarfile
import tempfile
from pathlib import Path

MACHINE = {"x86_64": 0x8664, "arm64ec": 0x8664, "i686": 0x14c}


def pe_stub(machine: int, arm64x: bool) -> bytes:
    buf = bytearray(0x200)
    buf[0:2] = b"MZ"
    struct.pack_into("<I", buf, 0x3C, 0x80)
    buf[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<H", buf, 0x84, machine)
    if arm64x:
        buf += b"\0arm64x\0"
    return bytes(buf)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--type", default="DXVK")
    ap.add_argument("--version-name", default="0.1.0-test")
    ap.add_argument("--system32-arch", default="arm64ec", choices=["arm64ec", "x86_64", "i686"])
    ap.add_argument("--no-marker", action="store_true")
    a = ap.parse_args()

    dlls = ["d3d11.dll", "d3d9.dll", "dxgi.dll", "d3d8.dll", "d3d10core.dll"]
    files = []

    with tempfile.TemporaryDirectory() as tmp:
        stage = Path(tmp)
        (stage / "system32").mkdir()
        (stage / "syswow64").mkdir()

        s32 = pe_stub(MACHINE[a.system32_arch], arm64x=(a.system32_arch == "arm64ec"))
        s64 = pe_stub(MACHINE["i686"], arm64x=False)

        for name in dlls:
            (stage / "system32" / name).write_bytes(s32)
            (stage / "syswow64" / name).write_bytes(s64)
            files.append({"source": f"system32/{name}", "target": "${system32}/" + name})
            files.append({"source": f"syswow64/{name}", "target": "${syswow64}/" + name})

        if not a.no_marker:
            (stage / "system32" / "dxvm-marker.txt").write_text(a.version_name + "\n")

        profile = {"type": a.type, "versionName": a.version_name,
                   "versionCode": 1, "description": "fixture", "files": files}
        (stage / "profile.json").write_text(json.dumps(profile, indent=2))

        tar_path = stage / "c.tar"
        with tarfile.open(tar_path, "w") as tf:
            tf.add(stage / "profile.json", arcname="profile.json")
            tf.add(stage / "system32", arcname="system32")
            tf.add(stage / "syswow64", arcname="syswow64")

        with open(tar_path, "rb") as f:
            data = f.read()

    packed = subprocess.run(["zstd", "-q", "-19", "-c"], input=data, capture_output=True, check=True)
    Path(a.out).write_bytes(packed.stdout)
    print(f"wrote {a.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
