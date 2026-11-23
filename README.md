# RETRO HANGUL — 한글패치 아카이브

게임별 한글패치, 버전 정보, 적용 안내와 한국어 오류 제보를 한곳에서 관리합니다.

**[홈페이지](https://bambabo-cmd.github.io/retro-korean-patches/)** · **[패치 목록](https://bambabo-cmd.github.io/retro-korean-patches/#library)** · **[오류 제보](https://github.com/bambabo-cmd/retro-korean-patches/issues/new/choose)** · **[버전 기록](https://bambabo-cmd.github.io/retro-korean-patches/versions.html)**

## 등록 프로젝트
- **NDS 스커지: 하이브** — `WIP011` · [저장소](https://github.com/bambabo-cmd/NDS-ScurgeHive)
- **GBC 에스트폴리스 전기: 되살아나는 전설** — `v0.9 / WIP51` · [저장소](https://github.com/bambabo-cmd/GBC-EstpolisYomigaeruDensetsu)
- **GBC 포욘의 던전룸 2** — `r39` · [저장소](https://github.com/bambabo-cmd/GBC-PoyonsDungeonRoom2)
- **GB 천지를 먹다** — `v0.9.4` · [저장소](https://github.com/bambabo-cmd/GB-TenchiWoKurau)
- **SFC 카오스 시드: 풍수회랑기** — `v0.7 / WIP68` · [저장소](https://github.com/bambabo-cmd/SFC-ChaosSeed)
- **SFC 에스트폴리스 전기 II** — `v0.9.8 / WIP206` · [저장소](https://github.com/bambabo-cmd/SFC-EstpolisDenki2)
- **NDS 프론트 미션 1st** — `v0.8.5 / WIP013` · [저장소](https://github.com/bambabo-cmd/NDS-FrontMission1st)
- **SFC 실바 사가 II** — `WIP348` · [저장소](https://github.com/bambabo-cmd/SFC-SilvaSaga2)
- **GBA 에스트폴리스 외전: 침묵의 유적** — `v0.9 / WIP0129` · [저장소](https://github.com/bambabo-cmd/GBA-EstpolisGaiden)
- **SFC 미스틱 아크** — `v1.1.12-19r19` · [저장소](https://github.com/bambabo-cmd/SFC-MysticArk)
- **SFC 초마법대륙 WOZZ** — `WIP135` · [저장소](https://github.com/bambabo-cmd/SFC-ChouMahouTairikuWOZZ)
- **SFC 대괴수 이야기 II** — `v0.9.8 / WIP121` · [저장소](https://github.com/bambabo-cmd/SFC-DaikaijuuMonogatari2)
- **NDS 세븐스 드래곤** — `v0.9.9A / WIP271` · [저장소](https://github.com/bambabo-cmd/NDS-7thDragon)
- **GBC 스타오션: 블루스피어** — `v0.9.5 / WIP172` · [저장소](https://github.com/bambabo-cmd/GBC-StarOceanBlueSphere)

위 버전은 2026-11-02 기준 각 저장소 README에 기록된 버전입니다. GitHub Releases가 등록되지 않은 저장소에서는 기존 README의 배포 파일을 확인합니다.

## 오류 제보
`.github/ISSUE_TEMPLATE/`에 한국어 양식(실행 오류, 번역/폰트, 개선 제안)이 있습니다. GitHub 계정 인증은 실명 확인을 뜻하지 않습니다.

## 배포 안내
기존 배포 파일은 로그인 없이 내려받을 수 있습니다. 게임별 파일·원본 해시·적용 방법은 각 저장소 README를 기준으로 합니다. 새 GitHub Releases는 별도 버전 기록에 추가합니다.

원본 ROM 및 게임 파일은 제공하지 않습니다.

## 관리 파일
- `docs/index.html`: GitHub Pages 홈페이지
- `docs/patches.json`: 14개 게임의 버전·공개 다운로드 링크
- `.github/ISSUE_TEMPLATE/`: 오류 제보 양식
- `templates/RELEASE_TEMPLATE_KO.md`: 릴리스 작성 양식

**배포 설정:** Settings → Pages → Deploy from a branch → main /docs → Save

## 릴리스 관리
- [현재 버전의 GitHub Releases](https://github.com/bambabo-cmd/retro-korean-patches/releases)
- [버전별 배포 운영 방법](RELEASE_MANAGEMENT.md)

## 에뮬레이터 개선 작업

- **[Gens KO 2.14 R6 시험 배포](https://github.com/bambabo-cmd/retro-korean-patches/releases/tag/gens-ko-2.14-r6-20261123-01)** — RetroArch GLSL·Slang 셰이더, CRT Mattias, 하위 폴더 검색, 매개변수 및 다중 패스를 지원합니다. R5의 메가 CD·BIOS 자동 검색 기능을 유지합니다.
- **[Windows 실행 ZIP](https://github.com/bambabo-cmd/retro-korean-patches/releases/download/gens-ko-2.14-r6-20261123-01/Gens_KO_R6_20261123_01_Windows.zip)** · **[전체 소스 ZIP](https://github.com/bambabo-cmd/retro-korean-patches/releases/download/gens-ko-2.14-r6-20261123-01/Gens_KO_R6_20261123_01_Source.zip)** — 설치법, 지원 범위, 검증 결과와 SHA-256은 릴리스 설명에 있습니다.
- Windows ZIP 전체를 풀고 `Gens_KO.exe`를 실행합니다. 동봉한 `librashader.dll`과 `shaders` 폴더도 함께 두세요. Cg와 HDR 최종 출력은 지원하지 않습니다. Windows GUI·WGL 직접 실행 여부를 포함한 검증 범위를 릴리스에 명시했습니다.
- 과거 작업 기록: **[Gens KO 2.14 R3 신규 기능 모듈·셰이더](emulators/gens-ko/README.md)** · **[R3 Windows 시험 배포](https://github.com/bambabo-cmd/retro-korean-patches/releases/tag/gens-ko-2.14-r3-test-20261121-01)**
