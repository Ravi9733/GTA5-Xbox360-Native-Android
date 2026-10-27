#!/usr/bin/env python3
"""Audit a local SanRecomp/XenonRecomp output folder without uploading game data.

Usage:
  python3 tools/audit_recomp_output.py /path/to/local/recomp-output

The report contains filenames and structural signals only; it does not copy or upload
any game dump or generated source. Generated output is not automatically playable.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

SOURCE_SUFFIXES = {".cpp", ".cc", ".cxx"}
HEADER_SUFFIXES = {".h", ".hpp", ".hh", ".hxx"}
MANIFEST_NAMES = {
    "CMakeLists.txt", "meson.build", "compile_commands.json",
    "sources.cmake", "generated.cmake", "function_table.cpp",
    "function_table.h", "functions.h", "functions.cpp",
}
MAX_FILES = 200_000


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path, help="local generated-output directory")
    args = parser.parse_args()
    root = args.root.expanduser().resolve()
    if not root.is_dir():
        parser.error(f"not a directory: {root}")

    cpp: list[str] = []
    headers: list[str] = []
    manifests: list[str] = []
    xex_files: list[str] = []
    god_markers: list[str] = []
    visited = 0

    for path in root.rglob("*"):
        if not path.is_file():
            continue
        visited += 1
        if visited > MAX_FILES:
            break
        rel = path.relative_to(root).as_posix()
        suffix = path.suffix.lower()
        if suffix in SOURCE_SUFFIXES:
            cpp.append(rel)
        elif suffix in HEADER_SUFFIXES:
            headers.append(rel)
        if path.name in MANIFEST_NAMES or path.name.lower().endswith((".cmake", ".json")):
            manifests.append(rel)
        if path.name.lower() == "default.xex":
            xex_files.append(rel)
        if path.suffix.lower() in {".god", ".iso"} or "god" in path.name.lower():
            god_markers.append(rel)

    report = {
        "root": str(root),
        "files_scanned": min(visited, MAX_FILES),
        "scan_limit_reached": visited > MAX_FILES,
        "generated_cpp_count": len(cpp),
        "generated_header_count": len(headers),
        "build_or_registry_metadata_count": len(manifests),
        "default_xex_found": bool(xex_files),
        "god_or_iso_named_entries_found": len(god_markers),
        "sample_cpp_paths": cpp[:40],
        "sample_header_paths": headers[:40],
        "build_or_registry_metadata_paths": manifests[:80],
        "default_xex_paths": xex_files[:10],
        "note": (
            "This is a structural inventory only. C++ files may be unrelated; "
            "confirm XenonRecomp/SanRecomp provenance and registry/import metadata "
            "before integrating them into the Android ARM64 build. No files were changed."
        ),
    }
    print(json.dumps(report, indent=2))
    if not cpp:
        return 2
    if not headers or not manifests:
        return 3
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
