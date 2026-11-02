# 버전별 배포 관리

이 저장소의 GitHub Releases에 게임별 한글패치 파일을 공개 아카이브로 보관합니다.

- [전체 배포 기록](https://github.com/bambabo-cmd/retro-korean-patches/releases)
- [버전 기록 홈페이지](https://bambabo-cmd.github.io/retro-korean-patches/versions.html)

## 현재 공개본의 최초 등록

.github/workflows/import-current-patches.yml에 정의된 워크플로가 docs/patches.json의 현재 파일을 검증하여 등록합니다.

- 태그 예: archive/NDS-7thDragon/v0.9.9A-WIP271
- 버전별 xdelta/IPS 파일 및 PATCH_INFO.json (존재하는 경우)을 첨부
- 개발판은 Pre-release(시험 배포)로 구분
- 등록된 태그가 있으면 중복 발행하지 않음
- 원본 ROM은 업로드하지 않음

## 다음 버전 배포 방법

1. 게임별 저장소에 새로운 패치와 검증 정보(원본 해시 등)를 업로드합니다.
2. docs/patches.json의 해당 게임 version/date/downloads를 새 파일명으로 수정합니다.
3. 중앙 저장소 Actions → 공개 패치 버전 아카이브 → Run workflow를 선택합니다.
4. 생성된 Release에서 첨부 파일, 태그, SHA-256, 주의사항을 확인합니다.
5. 버전 기록 홈페이지에서는 GitHub Releases가 조회되면 자동으로 보여줍니다.

## 주의

원본 README의 작업 기록일과 GitHub Release 발행일은 다릅니다.
과거에 존재했던 버전의 실제 파일을 확인하지 못하면 새 버전을 임의로 만들지 않습니다.
