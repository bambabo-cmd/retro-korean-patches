#!/usr/bin/env python3
"""Mirror current publicly available patch files as central GitHub pre-releases."""
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time
from urllib.parse import urlsplit
from urllib.request import Request, urlopen

ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "docs" / "patches.json"
OWNER = "bambabo-cmd"
MAX_SIZE = 20 * 1024 * 1024
EXTENSIONS = {".xdelta", ".ips", ".bps", ".ups"}


def fetch_bytes(url: str, limit: int = MAX_SIZE) -> bytes:
    req = Request(url, headers={"User-Agent": "retro-hangul-archive/1.0"})
    last_error = None
    for attempt in range(3):
        try:
            with urlopen(req, timeout=45) as response:
                payload = response.read(limit + 1)
            if len(payload) > limit:
                raise ValueError("파일 크기 제한 초과")
            return payload
        except (OSError, TimeoutError) as exc:
            last_error = exc
            if attempt < 2:
                time.sleep(2 * (attempt + 1))
    raise RuntimeError(f"다운로드 실패: {url} / {last_error}")


def source_url(project: str, uri: str) -> tuple[str, str]:
    u = urlsplit(uri)
    parts = u.path.split("/")
    expected = ["", OWNER, project, "raw", "refs", "heads", "main"]
    if u.scheme != "https" or u.netloc != "github.com" or parts[:7] != expected:
        raise ValueError("등록된 원본 저장소 이외의 URL은 허용되지 않음")
    if len(parts) < 8 or any(x in (".", "..", "") for x in parts[7:]):
        raise ValueError("패치 파일 경로가 올바르지 않음")
    from urllib.parse import unquote
    name = unquote(parts[-1])
    if not name or Path(name).suffix.lower() not in EXTENSIONS:
        raise ValueError("xdelta/IPS/BPS/UPS 이외의 파일은 배포하지 않음")
    raw = "https://raw.githubusercontent.com/" + "/".join(parts[1:3]) + "/main/" + "/".join(parts[7:])
    return raw, name


def validate_patch(name: str, payload: bytes) -> None:
    ext = Path(name).suffix.lower()
    if len(payload) < 16:
        raise ValueError("패치 파일이 비정상적으로 작음")
    if ext == ".xdelta" and not payload.startswith(bytes.fromhex("d6c3c4")):
        raise ValueError("유효한 VCDIFF 헤더가 아님")
    if ext == ".ips" and not payload.startswith(b"PATCH"):
        raise ValueError("유효한 IPS 헤더가 아님")


def gh(args: list[str], check: bool = True) -> subprocess.CompletedProcess:
    result = subprocess.run(["gh", *args], text=True, capture_output=True)
    if check and result.returncode:
        raise RuntimeError(f"gh error ({result.returncode}): {result.stderr.strip()[:1200]}")
    return result


def main() -> None:
    catalog = json.loads(CATALOG.read_text("utf-8"))
    failed = []
    created = 0
    for item in catalog:
        project = item["repository_url"].rstrip("/").split("/")[-1]
        version = item["version"]
        clean = re.sub(r"[^a-zA-Z0-9._-]+", "-", version).strip("-")
        if not re.fullmatch(r"[A-Za-z0-9._-]+", project) or not clean:
            failed.append((project, "버전/저장소명 오류"))
            continue
        tag = f"archive/{project}/{clean}"
        if gh(["release", "view", tag], check=False).returncode == 0:
            print(f"SKIP: 이미 등록된 릴리스 {tag}", flush=True)
            continue
        try:
            with tempfile.TemporaryDirectory() as directory:
                base = Path(directory)
                files = []
                sums = []
                used = set()
                for asset in item["downloads"]:
                    url, name = source_url(project, asset["url"])
                    if name in used:
                        raise ValueError("같은 이름의 파일 중복")
                    used.add(name)
                    payload = fetch_bytes(url)
                    validate_patch(name, payload)
                    # GitHub CLI treats [ and ] in filenames as glob patterns.
                    safe_name = name.replace("[", "_").replace("]", "_").replace("*", "_").replace("?", "_")
                    path = base / safe_name
                    path.write_bytes(payload)
                    files.append(str(path))
                    sums.append((name, len(payload), hashlib.sha256(payload).hexdigest()))
                if not files:
                    raise ValueError("배포할 패치 파일 없음")
                # Existing PATCH_INFO is attached without modification when available.
                info_url = f"https://raw.githubusercontent.com/{OWNER}/{project}/main/PATCH_INFO.json"
                try:
                    metadata = fetch_bytes(info_url, limit=256 * 1024)
                    json.loads(metadata.decode("utf-8"))
                    info = base / "PATCH_INFO.json"
                    info.write_bytes(metadata)
                    files.append(str(info))
                except (ValueError, RuntimeError, UnicodeError):
                    print(f"NOTICE: {project} PATCH_INFO.json 생략", flush=True)
                notes = [
                    "## 배포 정보",
                    f"- 게임: {item['platform']} · {item['title']}",
                    f"- 패치 버전: {version}",
                    f"- 검수 상태: {item['status']}",
                    f"- 원본 작업 기록일: {item['date']} (기존 README 기준)",
                    f"- 원본 저장소: {item['repository_url']}",
                    "",
                    "공개되어 있던 패치를 별도 변경 없이 아카이브에 보관한 시험 배포입니다.",
                    "이 릴리스의 발행일은 원래 제작·검수 완료일을 뜻하지 않습니다.",
                    "게임 전체 완주·실기 검증 완료를 보증하지 않습니다.",
                    "지원 원본 ROM의 판본·체크섬과 저장 호환성은 원본 저장소 README를 확인하세요.",
                    "원본 ROM 파일은 제공하지 않습니다.",
                    "",
                    "## 패치 파일 SHA-256",
                ]
                for name, size, digest in sums:
                    notes += [f"- {name} ({size:,}바이트): " + chr(96) + digest + chr(96)]
                note_path = base / "NOTES.md"
                note_path.write_text("\n".join(notes) + "\n", encoding="utf-8")
                title = f"{item['platform']} {item['title']} · {version}"
                cmd = ["release", "create", tag, *files, "--title", title,
                       "--notes-file", str(note_path), "--target", "main",
                       "--prerelease", "--latest=false"]
                result = gh(cmd)
                print(f"CREATED: {tag} {result.stdout.strip()}", flush=True)
                created += 1
                time.sleep(1)
        except Exception as exc:
            failed.append((project, str(exc)))
            print(f"ERROR: {project} {exc}", file=sys.stderr, flush=True)
    print(f"새 릴리스 {created}개, 실패 {len(failed)}개", flush=True)
    if failed:
        for project, error in failed:
            print(f"FAILED: {project}: {error}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
