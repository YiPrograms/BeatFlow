#!/usr/bin/env python3
"""Build a reviewable QMOD, detached debug symbols, and SHA-256 checksums."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import zipfile


ROOT = Path(__file__).resolve().parents[1]


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def load_manifest() -> dict[str, object]:
    qpm = json.loads((ROOT / "qpm.json").read_text(encoding="utf-8"))
    manifest = json.loads((ROOT / "mod.template.json").read_text(encoding="utf-8"))
    manifest["version"] = qpm["info"]["version"]
    manifest["lateModFiles"] = [qpm["info"]["additionalData"]["overrideSoName"]]
    manifest["$schema"] = (
        "https://raw.githubusercontent.com/Lauriethefish/QuestPatcher.QMod/refs/heads/main/"
        "QuestPatcher.QMod/Resources/qmod.schema.json"
    )
    return manifest


def find_llvm_tool(name: str) -> str:
    configured = os.environ.get("ANDROID_NDK_HOME")
    if not configured and (ROOT / "ndkpath.txt").exists():
        configured = (ROOT / "ndkpath.txt").read_text(encoding="utf-8").strip()
    if configured:
        tool_root = Path(configured) / "toolchains/llvm/prebuilt"
        candidates = sorted(tool_root.glob(f"*/bin/{name}")) + sorted(tool_root.glob(f"*/bin/{name}.exe"))
        if candidates:
            return str(candidates[0])
    resolved = shutil.which(name)
    if resolved:
        return resolved
    raise SystemExit(f"Could not find {name}; set ANDROID_NDK_HOME or run qpm restore.")


def run(*arguments: str) -> None:
    subprocess.run(arguments, check=True)


def write_qmod(path: Path, manifest: dict[str, object], library: Path) -> None:
    timestamp = (2026, 1, 1, 0, 0, 0)
    with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        manifest_info = zipfile.ZipInfo("mod.json", timestamp)
        manifest_info.external_attr = 0o644 << 16
        archive.writestr(manifest_info, json.dumps(manifest, indent=2).encode() + b"\n")
        library_info = zipfile.ZipInfo(library.name, timestamp)
        library_info.external_attr = 0o755 << 16
        archive.writestr(library_info, library.read_bytes())


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build/quest")
    parser.add_argument("--output-dir", type=Path, default=ROOT / "dist")
    arguments = parser.parse_args()

    build_dir = arguments.build_dir.resolve()
    output_dir = arguments.output_dir.resolve()
    source_library = build_dir / "libBeatFlow.so"
    if not source_library.is_file():
        raise SystemExit(f"Missing {source_library}; build the quest-release preset first.")

    manifest = load_manifest()
    version = str(manifest["version"])
    compatibility_source = ROOT / "release/compatibility.json"
    compatibility_data = json.loads(compatibility_source.read_text(encoding="utf-8"))
    if compatibility_data.get("version") != version:
        raise SystemExit("release/compatibility.json does not match the package version.")
    if compatibility_data.get("beatSaberPackageVersion") != manifest["packageVersion"]:
        raise SystemExit("release/compatibility.json does not match the Beat Saber package version.")
    output_dir.mkdir(parents=True, exist_ok=True)
    qmod = output_dir / f"BeatFlow-{version}.qmod"
    symbols = output_dir / f"libBeatFlow-{version}.so.debug"
    compatibility = output_dir / "compatibility.json"

    objcopy = find_llvm_tool("llvm-objcopy")
    strip = find_llvm_tool("llvm-strip")
    with tempfile.TemporaryDirectory(prefix="beatflow-package-") as temporary:
        stripped = Path(temporary) / "libBeatFlow.so"
        shutil.copy2(source_library, stripped)
        run(objcopy, "--only-keep-debug", str(source_library), str(symbols))
        run(strip, "--strip-unneeded", str(stripped))
        run(objcopy, f"--add-gnu-debuglink={symbols}", str(stripped))
        write_qmod(qmod, manifest, stripped)
    shutil.copy2(compatibility_source, compatibility)

    checksum_file = output_dir / "SHA256SUMS"
    artifacts = (qmod, symbols, compatibility)
    checksum_file.write_text(
        "".join(f"{sha256(path)}  {path.name}\n" for path in artifacts), encoding="utf-8"
    )
    print(qmod)
    print(symbols)
    print(compatibility)
    print(checksum_file)


if __name__ == "__main__":
    main()
