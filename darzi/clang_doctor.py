#!/usr/bin/env python3
"""
clang_doctor.py – sanity-check Clang / libclang / Python bindings in a Conda env.

Run:  python clang_doctor.py
"""

import os
import subprocess
import re
import sys
from importlib import metadata
from pathlib import Path

report = []                         # collect lines → print once at end
ok     = True                       # overall status flag
want   = 18                         # *** target major version ***


# ---------------------------------------------------------------------------
# Helper utilities
# ---------------------------------------------------------------------------
def fmt(path):
    return str(path) if path else "not found"

def find_lib(prefix: Path, stem: str):
    """Return Path to the first lib matching lib<stem>.so.* inside prefix/lib."""
    libdir = prefix / "lib"
    if not libdir.exists():
        return None
    for f in libdir.iterdir():
        if f.name.startswith(f"lib{stem}.so"):
            return f
    return None

def major_from_filename(path: Path):
    m = re.search(r"\.so\.(\d+)", path.name)
    return int(m.group(1)) if m else None

def run(cmd):
    try:
        out = subprocess.check_output(cmd, stderr=subprocess.STDOUT,
                                      text=True, timeout=5)
        return out.strip()
    except Exception as e:
        return f"<{e.__class__.__name__}: {e}>"

def major_from_version_string(text):
    m = re.search(r"clang version\s+(\d+)\.", text)
    return int(m.group(1)) if m else None

def pkg_version(name):
    try:
        return metadata.version(name)
    except metadata.PackageNotFoundError:
        return None

# ---------------------------------------------------------------------------
# 1) Conda prefix
# ---------------------------------------------------------------------------
prefix = os.environ.get("CONDA_PREFIX")
if not prefix:
    report.append("❌  Not inside a Conda environment (CONDA_PREFIX undefined)")
    ok = False
else:
    prefix = Path(prefix)
    report.append(f"🛈  Conda prefix : {prefix}")

# ---------------------------------------------------------------------------
# 2) clang / clang++ executables
# ---------------------------------------------------------------------------
for exe in ("clang", "clang++"):
    out = run([exe, "--version"])
    ver = major_from_version_string(out)
    if ver:
        status = "✅" if ver == want else "⚠"
        report.append(f"{status}  {exe:<7}: version {ver}  ({'matches' if ver == want else 'mismatch'})")
    else:
        report.append(f"❌  {exe:<7}: {out}")
        ok = False

# ---------------------------------------------------------------------------
# 3) Shared libraries inside the env
# ---------------------------------------------------------------------------
libs = {}
if prefix:
    for stem in ("clang", "clang-cpp"):
        path = find_lib(prefix, stem)
        libs[stem] = path
        if path:
            maj = major_from_filename(path)
            status = "✅" if maj == want else "⚠"
            report.append(f"{status}  lib{stem}.so: {path.name} (major {maj})")
            if maj != want:
                ok = False
        else:
            report.append(f"❌  lib{stem}.so: not found")
            ok = False

# ---------------------------------------------------------------------------
# 4) Python packages (clang, libclang)
# ---------------------------------------------------------------------------
for pkg in ("libclang", "clang"):
    ver = pkg_version(pkg)
    if ver:
        maj = int(ver.split(".")[0])
        status = "✅" if maj == want else "⚠"
        report.append(f"{status}  python pkg {pkg:<8}: {ver} (major {maj})")
        if maj != want:
            ok = False
    else:
        report.append(f"❌  python pkg {pkg:<8}: not installed")

# ---------------------------------------------------------------------------
# 5) Try importing and loading libclang
# ---------------------------------------------------------------------------
try:
    import clang.cindex as cl
    loaded = False
    try:
        # let the binding try auto-discovery first
        cl.Config.set_library_path(str(prefix / "lib"))
        idx = cl.Index.create()
        loaded = True
        path_loaded = cl.Config.loaded_library_file
    except Exception as e:
        path_loaded = f"<failed: {e}>"
    has_value = hasattr(cl.Type, "get_template_argument_value")
    report.append(f"{'✅' if loaded else '❌'}  clang.cindex loaded : {path_loaded}")
    status = "✅" if has_value else "⚠"
    report.append(f"{status}  Type.get_template_argument_value present: {has_value}")
    if not loaded or not has_value:
        ok = False
except ImportError as e:
    report.append(f"❌  import clang.cindex failed: {e}")
    ok = False

# ---------------------------------------------------------------------------
# Final summary
# ---------------------------------------------------------------------------
print("\n".join(report))
print()
print("Overall:", "✅  OK – all major numbers match" if ok else "⚠  Fix the mismatches above")
if not ok:
    print(f"Target: every component should report major **{want}**")