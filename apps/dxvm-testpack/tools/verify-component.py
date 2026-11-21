#!/usr/bin/env python3
"""Verify a PanPlay component archive against the contract PanPlay's unpacker enforces.

Contract source: Contents.kt (archive shape, profile.json limits, type/versionName
sanitising) and Containers.kt (${system32}/${syswow64} install targets).

Exit 0 only when every check passes. Each check prints one line.
"""
import io
import json
import struct
import subprocess
import sys
import tarfile

PROFILE_MAX_BYTES = 64 * 1024
DXVK_TYPE = "DXVK"
MARKER = "system32/dxvm-marker.txt"
MACHINE_I686 = 0x14C
MACHINE_X64_HDR = 0x8664

failures = []


def check(ok: bool, label: str, detail: str = "") -> None:
    print(("ok   " if ok else "FAIL ") + label + (f": {detail}" if detail else ""))
    if not ok:
        failures.append(label)


def pe_machine(blob: bytes) -> int:
    if len(blob) < 0x100 or blob[:2] != b"MZ":
        return -1
    off = struct.unpack_from("<I", blob, 0x3C)[0]
    if off + 6 > len(blob) or blob[off:off + 4] != b"PE\0\0":
        return -1
    return struct.unpack_from("<H", blob, off + 4)[0]


def is_arm64ec(blob: bytes) -> bool:
    # ARM64EC images carry the x64-compatible header machine type plus arm64x
    # runtime metadata, which is what llvm-mingw's arm64ec target emits.
    return pe_machine(blob) == MACHINE_X64_HDR and b"arm64x" in blob


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        print("usage: verify-component.py <component.wcp>", file=sys.stderr)
        return 2

    path = argv[1]
    raw = open(path, "rb").read()

    try:
        tar_bytes = subprocess.run(["zstd", "-dc"], input=raw,
                                   capture_output=True, check=True).stdout
    except subprocess.CalledProcessError as exc:
        check(False, "archive is zstd", exc.stderr.decode()[:120])
        return 1

    check(True, "archive is zstd")

    members: dict[str, bytes] = {}
    try:
        with tarfile.open(fileobj=io.BytesIO(tar_bytes)) as tf:
            for m in tf.getmembers():
                if m.isfile():
                    members[m.name.lstrip("./")] = tf.extractfile(m).read()
    except tarfile.TarError as exc:
        check(False, "archive is tar", str(exc)[:120])
        return 1

    check(True, "archive is tar")

    # --- profile.json ---
    if "profile.json" not in members:
        check(False, "profile.json at archive root")
        return 1
    check(True, "profile.json at archive root")

    profile_raw = members["profile.json"]
    check(len(profile_raw) <= PROFILE_MAX_BYTES, "profile.json <= 64 KiB",
          f"{len(profile_raw)} bytes")

    try:
        profile = json.loads(profile_raw)
    except json.JSONDecodeError as exc:
        check(False, "profile.json parses", str(exc)[:80])
        return 1
    check(True, "profile.json parses")

    ptype = str(profile.get("type", ""))
    version = str(profile.get("versionName", ""))
    check(ptype == DXVK_TYPE, "type is DXVK", f"got {ptype!r}")
    check(bool(version) and version not in (".", ".."),
          "versionName present and non-empty", f"got {version!r}")

    # --- files[] mapping ---
    files = profile.get("files") or []
    check(isinstance(files, list) and len(files) > 0, "files[] is non-empty",
          f"{len(files)} entries")

    missing_src = [f.get("source", "") for f in files
                   if f.get("source") and f.get("source").lstrip("./") not in members]
    check(not missing_src, "every files[] source exists", ", ".join(missing_src[:4]))

    bad_target = [f.get("target", "") for f in files
                  if not (str(f.get("target", "")).startswith("${system32}/")
                          or str(f.get("target", "")).startswith("${syswow64}/"))]
    check(not bad_target, "every target uses ${system32} or ${syswow64}",
          ", ".join(bad_target[:3]))

    # --- architectures: system32 must be arm64ec, syswow64 i686 ---
    s32 = [n for n in members if n.startswith("system32/") and n.endswith(".dll")]
    s64 = [n for n in members if n.startswith("syswow64/") and n.endswith(".dll")]

    check(bool(s32), "system32 has DLLs", f"{len(s32)}")
    check(bool(s64), "syswow64 has DLLs", f"{len(s64)}")

    not_ec = [n for n in s32 if not is_arm64ec(members[n])]
    check(not not_ec, "system32 DLLs are arm64ec",
          "not arm64ec: " + ", ".join(not_ec[:4]) if not_ec else f"{len(s32)} files")

    not_i686 = [n for n in s64 if pe_machine(members[n]) != MACHINE_I686]
    check(not not_i686, "syswow64 DLLs are i686",
          "not i686: " + ", ".join(not_i686[:4]) if not_i686 else f"{len(s64)} files")

    # --- DXVM marker: proves the prefix gets DXVM, not stock DXVK ---
    check(MARKER in members, "dxvm marker present", MARKER)

    if failures:
        print(f"\n{len(failures)} check(s) failed")
        return 1

    print("\ncomponent OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
