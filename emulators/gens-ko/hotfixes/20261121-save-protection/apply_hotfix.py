#!/usr/bin/env python3
"""Apply the pinned save.cpp patch to an existing complete Gens KO R3 tree.

The patch is applied and verified in a temporary directory before replacing
the user's source file. Requires Python 3 and Git; does not access the network.
GPL-2.0; see ../../licenses/Gens_original_LICENSE.txt.
"""
from pathlib import Path
import argparse
import hashlib
import os
import shutil
import subprocess
import tempfile

RELATIVE = Path("src/common/src/save.cpp")
BEFORE = "73bb7f4d9b6c396fb2cb889ce3a1ba9859f8a10591f65581a0d7f00b838a454e"
AFTER = "ebb696fb80583d47001e5c1c87a330d4756d762c4898bc78c4c24ed3f5de9b42"
PATCH_SHA256 = "d93a4f4b603efbfcf64f274930affd334f9a3d1632d31c0bfc3d33ad85c28c2f"


def digest(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source_root", type=Path, help="Complete Gens_KO_source directory")
    parser.add_argument("--check", action="store_true", help="Verify application without changing the source")
    args = parser.parse_args()
    root = args.source_root.expanduser().resolve(strict=True)
    candidate = root / RELATIVE
    target = candidate.resolve(strict=True)
    if candidate.is_symlink() or not target.is_file():
        raise SystemExit("Refusing a symlink or non-file source target.")
    try:
        target.relative_to(root)
    except ValueError:
        raise SystemExit("Refusing a source target outside the selected source tree.")
    patch = Path(__file__).resolve().with_name("save-protection.patch")
    patch_bytes = patch.read_bytes()
    if digest(patch_bytes) != PATCH_SHA256:
        raise SystemExit("Patch SHA-256 mismatch; no source changed.")
    original = target.read_bytes()
    if digest(original) == AFTER:
        print("Already applied:", target)
        return
    if digest(original) != BEFORE:
        raise SystemExit("Source SHA-256 mismatch. This patch requires the unmodified R3 TEST 20261120_03 save.cpp; no source changed.")
    git = shutil.which("git")
    if not git:
        raise SystemExit("Git is required to validate the unified patch; no source changed.")
    with tempfile.TemporaryDirectory(prefix="gens-save-hotfix-") as directory:
        work = Path(directory)
        staged = work / RELATIVE
        staged.parent.mkdir(parents=True)
        staged.write_bytes(original)
        local_patch = work / "hotfix.patch"
        local_patch.write_bytes(patch_bytes)
        for flags in (["--check"], []):
            result = subprocess.run([git, "apply", "--whitespace=nowarn", *flags, str(local_patch)],
                                    cwd=work, capture_output=True, text=True)
            if result.returncode:
                raise SystemExit("Patch validation failed; no source changed.\n" + result.stderr)
        updated = staged.read_bytes()
    if digest(updated) != AFTER:
        raise SystemExit("Patched source SHA-256 mismatch; no source changed.")
    if args.check:
        print("PASS: exact source, patch, and output hashes verified; no source changed.")
        return
    if target.read_bytes() != original:
        raise SystemExit("Source changed during verification; no source replaced.")
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=target.parent, prefix="save.cpp.hotfix-", delete=False) as file:
            temporary = Path(file.name)
            file.write(updated)
            file.flush()
            os.fsync(file.fileno())
        shutil.copymode(target, temporary)
        os.replace(temporary, target)
        temporary = None
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
    print("Applied:", target)
    print("SHA-256:", AFTER)


if __name__ == "__main__":
    main()
