#!/usr/bin/env python3
"""Exercise the production SRAM file functions with native file API doubles.

The exact save.cpp implementation is compiled below. Windows file/current-
directory/UI calls are replaced by local stdio calls and controlled failures;
this is not a Windows GUI or real Windows filesystem validation. All battery
save inputs are generated in a temporary directory. No game ROM is needed.
"""
from pathlib import Path
from datetime import datetime, timezone
import argparse
import hashlib
import json
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "src/common/src/save.cpp"

PREFIX = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
typedef FILE *HANDLE;
typedef int BOOL;
typedef unsigned long DWORD;
struct BY_HANDLE_FILE_INFORMATION {
    uint32_t nFileSizeHigh, nFileSizeLow;
};
#define INVALID_HANDLE_VALUE ((FILE *)0)
#define INVALID_FILE_ATTRIBUTES (~0UL)
#define GENERIC_WRITE 1
#define GENERIC_READ 2
#define FILE_SHARE_READ 4
#define CREATE_ALWAYS 1
#define OPEN_EXISTING 2
#define FILE_ATTRIBUTE_NORMAL 0
#define MOVEFILE_REPLACE_EXISTING 1
#define MOVEFILE_WRITE_THROUGH 2
#define ERROR_FILE_NOT_FOUND 2
#define ERROR_ACCESS_DENIED 5
#define FALSE 0
#define MB_OK 0
#define MB_ICONERROR 16
enum Fault { NONE, OPEN_DENIED, INFO_ERROR, READ_ERROR, SHORT_READ,
             READ_CLOSE_ERROR, WRITE_ERROR, RENAME_ERROR, CWD_ERROR };
static Fault fault;
static unsigned long last_error;
static unsigned warnings, write_attempts, checks;
static int pier = 1, HWnd;
static unsigned nv;
static unsigned char SRAM[65536];
static char SRAM_Dir[1024] = "", Rom_Name[512] = "slot";
static char Gens_Path[1024] = ".", Str_Tmp[4096];
static int Cart_IsPier() { return pier; }
static void Cart_SetNvStatus(unsigned value) { nv = value; }
static unsigned Cart_GetNvStatus() { return nv; }
static BOOL SetCurrentDirectory(const char *) { return fault != CWD_ERROR; }
static void Put_Info(const char *, int) {}
static int MessageBoxW(int, const wchar_t *, const wchar_t *, unsigned) {
    ++warnings; return 1;
}
static DWORD GetLastError() { return last_error; }
static HANDLE CreateFile(const char *name, int access, int, void *, int, int, void *) {
    if (access == GENERIC_READ && fault == OPEN_DENIED) {
        last_error = ERROR_ACCESS_DENIED; return INVALID_HANDLE_VALUE;
    }
    if (access == GENERIC_WRITE) ++write_attempts;
    FILE *file = fopen(name, access == GENERIC_WRITE ? "wb" : "rb");
    last_error = file ? 0 : (errno == ENOENT ? ERROR_FILE_NOT_FOUND : ERROR_ACCESS_DENIED);
    return file;
}
static BOOL GetFileInformationByHandle(HANDLE file, BY_HANDLE_FILE_INFORMATION *info) {
    struct stat data;
    if (fault == INFO_ERROR || fstat(fileno(file), &data)) return 0;
    info->nFileSizeLow = (uint32_t)data.st_size;
    info->nFileSizeHigh = (uint32_t)((uint64_t)data.st_size >> 32);
    return 1;
}
static BOOL ReadFile(HANDLE file, void *data, unsigned long size, unsigned long *read, void *) {
    if ((fault == READ_ERROR || fault == SHORT_READ) && size > 128) size = 128;
    *read = fread(data, 1, size, file);
    return fault != READ_ERROR && !ferror(file);
}
static BOOL WriteFile(HANDLE file, const void *data, unsigned long size, unsigned long *written, void *) {
    if (fault == WRITE_ERROR) { *written = 0; return 0; }
    *written = fwrite(data, 1, size, file); return !ferror(file);
}
static BOOL FlushFileBuffers(HANDLE file) {
    return fflush(file) == 0 && fsync(fileno(file)) == 0;
}
static BOOL CloseHandle(HANDLE file) {
    const int result = fclose(file);
    return fault != READ_CLOSE_ERROR && result == 0;
}
static BOOL MoveFileEx(const char *from, const char *to, int) {
    return fault != RENAME_ERROR && rename(from, to) == 0;
}
static BOOL DeleteFile(const char *name) { return remove(name) == 0; }
static unsigned long GetFileAttributes(const char *name) {
    struct stat info; return stat(name, &info) == 0 ? 0 : INVALID_FILE_ATTRIBUTES;
}
static void check(bool value, const char *description) {
    ++checks;
    if (!value) { fprintf(stderr, "FAIL: %s\n", description); exit(1); }
}
static std::vector<unsigned char> contents(const char *name) {
    FILE *file = fopen(name, "rb");
    if (!file) return {};
    std::vector<unsigned char> data;
    int value;
    while ((value = fgetc(file)) != EOF) data.push_back(value);
    fclose(file); return data;
}
static void fixture(const char *name, const std::vector<unsigned char> &data) {
    FILE *file = fopen(name, "wb");
    if (!file) exit(2);
    if ((!data.empty() && fwrite(data.data(), 1, data.size(), file) != data.size()) || fclose(file)) exit(2);
}
static std::vector<unsigned char> pattern(unsigned size) {
    std::vector<unsigned char> data(size);
    for (unsigned i = 0; i < size; ++i) data[i] = (i * 37 + 19) % 251;
    return data;
}
static bool filled(unsigned first, unsigned char value) {
    for (unsigned i = first; i < sizeof(SRAM); ++i) if (SRAM[i] != value) return false;
    return true;
}
'''

MAIN = r'''
static void rejected_load_preserves(const char *base, const std::vector<unsigned char> &data,
                                    Fault injected, const char *description) {
    char name[600]; snprintf(name, sizeof(name), "%s.srm", base);
    strcpy(Rom_Name, base); fixture(name, data);
    const unsigned old_warnings = warnings;
    fault = injected;
    check(!Load_SRAM(), description);
    check(warnings == old_warnings + 1, "failed load explains write protection to the user");
    fault = NONE;
    memset(SRAM, 0x43, sizeof(SRAM)); /* Simulate game initialization after failure. */
    const unsigned old_writes = write_attempts;
    check(!Save_SRAM() && write_attempts == old_writes,
          "failed load blocks data and SPI sidecar writes for the ROM session");
    check(contents(name) == data, "unread existing file remains byte-exact after attempted save");
    strcpy(SRAM_Dir, "./");
    check(!Save_SRAM() && contents(name) == data,
          "changing path spelling cannot bypass the failed-load session guard");
    SRAM_Dir[0] = 0;
}

int main() {
    const std::vector<unsigned char> full = pattern(65536), small = pattern(4096);
    fixture("slot.srm", full);
    check(Load_SRAM() && memcmp(SRAM, full.data(), full.size()) == 0, "64KiB raw Pier load is byte-exact");
    check(Save_SRAM() && contents("slot.srm") == full, "64KiB raw Pier round-trip is byte-exact");
    fixture("slot.srm", small);
    check(Load_SRAM() && memcmp(SRAM, small.data(), small.size()) == 0 && filled(4096, 255),
          "4KiB raw Pier save loads with FF padding and no byte swap");
    nv = 0x8c;
    check(Save_SRAM(), "Pier status sidecar saves");
    nv = 0;
    check(Load_SRAM() && nv == 0x8c, "Pier protection status round-trips separately");
    std::vector<unsigned char> padded(65536, 255);
    memcpy(padded.data(), small.data(), small.size());
    check(contents("slot.srm") == padded, "4KiB image expands without changing its EEPROM bytes");
    fixture("slot.srm", pattern(1024));
    check(Load_SRAM() && filled(1024, 255), "trimmed raw save remains supported");

    fixture("slot.srm", full);
    check(Load_SRAM(), "load before write-failure tests");
    SRAM[100] ^= 0xff;
    fault = WRITE_ERROR;
    check(!Save_SRAM() && contents("slot.srm") == full, "write failure preserves the previous file");
    fault = RENAME_ERROR;
    check(!Save_SRAM() && contents("slot.srm") == full, "atomic replacement failure preserves the previous file");
    fault = NONE;

    rejected_load_preserves("read_error", full, READ_ERROR, "read failure rejected");
    rejected_load_preserves("partial", full, SHORT_READ, "successful short read rejected");
    rejected_load_preserves("oversize", pattern(65537), NONE, "oversize save rejected instead of truncated");
    rejected_load_preserves("empty", {}, NONE, "empty save rejected instead of replacing it");
    rejected_load_preserves("denied", full, OPEN_DENIED, "access-denied load rejected");
    rejected_load_preserves("no_info", full, INFO_ERROR, "file-size query failure rejected");
    rejected_load_preserves("close_error", full, READ_CLOSE_ERROR, "read-handle close failure rejected");
    rejected_load_preserves("bad_directory", full, CWD_ERROR, "working-directory failure rejected");

    strcpy(Rom_Name, "read_error");
    check(Load_SRAM() && Save_SRAM() && contents("read_error.srm") == full,
          "successfully reopening the same ROM clears its failed-load guard");
    strcpy(Rom_Name, "new_version");
    const unsigned old_warnings = warnings;
    check(!Load_SRAM() && filled(0, 255) && warnings == old_warnings,
          "missing save is a normal new game without an error dialog");
    check(contents("slot.srm") == full, "ROM basename change does not alter the old save");
    SRAM[0] = 0x50; SRAM[1] = 0x53;
    check(Save_SRAM() && contents("new_version.srm").size() == 65536,
          "genuinely new Pier save can be created after a missing-file result");

    pier = 0; strcpy(Rom_Name, "ordinary"); fixture("ordinary.srm", small);
    check(Load_SRAM() && memcmp(SRAM, small.data(), small.size()) == 0 && filled(4096, 0),
          "ordinary cartridge retains zero-padding semantics");
    check(Save_SRAM() && contents("ordinary.srm") == small,
          "ordinary cartridge retains trimmed power-of-two save size");
    memset(SRAM, 0, sizeof(SRAM));
    check(Save_SRAM() && contents("ordinary.srm") == std::vector<unsigned char>(65536, 0),
          "intentionally cleared existing ordinary save remains persistable");
    strcpy(Rom_Name, "ordinary_new");
    check(!Load_SRAM() && !Save_SRAM() && GetFileAttributes("ordinary_new.srm") == INVALID_FILE_ATTRIBUTES,
          "ordinary cartridge without save data does not create an empty file");
    rejected_load_preserves("ordinary_error", small, READ_ERROR,
                            "ordinary cartridge read failure is also protected");
    printf("PASS: %u SRAM file safety/compatibility checks\n", checks);
    return 0;
}
'''


def digest(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, default=ROOT / "build/sram_io")
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    raw_source = SOURCE.read_bytes()
    source = raw_source.decode("utf-8")
    start = source.index("static int SRAM_Write_Blocked")
    end = source.index("\nvoid S_Format_BRAM(", start)
    program = PREFIX + source[start:end] + MAIN
    env = os.environ.copy()
    env["ASAN_OPTIONS"] = "detect_leaks=0"
    env["UBSAN_OPTIONS"] = "halt_on_error=1"
    with tempfile.TemporaryDirectory(prefix="gens-sram-io-") as directory:
        work = Path(directory)
        (work / "test.cpp").write_text(program)
        command = ["g++", "-std=c++11", "-O1", "-g", "-fsanitize=address,undefined",
                   "-fno-omit-frame-pointer", str(work / "test.cpp"), "-o", str(work / "test")]
        build = subprocess.run(command, capture_output=True, text=True)
        (args.out / "compile.log").write_text(build.stdout + build.stderr)
        if build.returncode:
            raise SystemExit(build.stdout + build.stderr)
        run = subprocess.run([str(work / "test")], cwd=work, env=env, capture_output=True, text=True)
        output = run.stdout + run.stderr
        (args.out / "test.log").write_text(output)
        match = re.search(r"PASS: (\d+) SRAM", output)
        report = {
            "schema": 1,
            "recorded_utc": datetime.now(timezone.utc).isoformat(),
            "status": "PASS" if run.returncode == 0 and match else "FAIL",
            "checks": int(match.group(1)) if match else 0,
            "source_sha256": digest(raw_source),
            "sources": {str(SOURCE.relative_to(ROOT)): digest(raw_source),
                        str(Path(__file__).resolve().relative_to(ROOT)): digest(Path(__file__).read_bytes())},
            "reproduce_command": "python3 tests/test_sram_io.py",
            "scope": "Exact production SRAM functions, native stdio-backed Windows file API doubles, injected I/O failures. No real Windows filesystem/GUI or game-save recovery claim.",
            "sanitizers": ["address", "undefined"],
            "game_rom_required": False,
            "existing_user_files_modified": False,
            "returncode": run.returncode,
        }
        (args.out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
        print(output, end="")
        print("Report:", args.out / "report.json")
        if report["status"] != "PASS":
            raise SystemExit(1)


if __name__ == "__main__":
    main()
