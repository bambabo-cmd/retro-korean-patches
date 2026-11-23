## MesenCE KO 2.2.1 R2 TEST · Windows x64

MesenCE 2.2.1 기반 비공식 수정판입니다. RetroArch 셰이더 폴더를 복사해서 사용할 수 있도록 화면 출력 기능을 추가했습니다. 실행판과 전체 수정 소스를 함께 제공합니다.

### 주요 기능

- RetroArch의 `shaders_glsl` / `shaders_slang` 폴더를 하위 구조와 참조 파일을 포함해 복사하여 사용합니다. `.glsl`, `.glslp`, `.slang`, `.slangp`를 지원합니다.
- 하위 폴더 검색, 매개변수 조절·저장, 다중 패스, LUT, `include`, 프리셋 참조를 지원합니다.
- 제공받은 원본 `crt-mattias.glsl`과 연결 프리셋을 포함합니다. 화면 곡률과 주사선 이동 속도를 조절할 수 있습니다.
- 한글 UI, 키보드·게임패드 일괄 설정, 메뉴 숨김, 전체화면·배속·되감기·슬로우모션 단축키를 유지합니다. 자동 업데이트 확인은 기본 끔입니다.
- 에뮬레이션 코어는 공식 MesenCE 2.2.1을 유지합니다.

### 실행 및 셰이더 사용

1. `Windows_x64.zip`을 새 폴더에 풀고 `Mesen.exe`를 실행합니다.
2. **설정 → 셰이더 / CRT 출력 → 셰이더 폴더 열기**를 선택합니다.
3. RetroArch 셰이더 폴더를 원래 구조대로 복사합니다. 프리셋만 따로 옮기면 참조 파일이 누락될 수 있습니다.
4. **목록 새로고침 → GPU 셰이더 사용 → 프리셋 선택 → 적용** 순서로 설정합니다.

R1 설정을 이어 쓰려면 R2를 새 폴더에 풀고 기존 `settings.json`, Saves, SaveStates, Cheats, GameConfig, Firmware 및 사용자 셰이더를 옮깁니다. 새 기본 설정으로 기존 `settings.json`을 덮어쓰지 마세요.

### TEST 범위와 제약

동봉된 검증 기록에는 Windows x64 빌드, Linux Mesa 환경의 출력·입력·설정 검사 및 Windows DLL 정적 검사가 포함되어 있습니다. **실제 Windows GPU에서의 게임 실행, 전체화면·디스플레이 전환, 물리 컨트롤러 실사용 검증은 완료하지 않았으므로 R2 TEST로 배포합니다.**

OpenGL 3.3 또는 OpenGL ES 3.0 이상이 필요합니다. 구형 Cg(`.cg`/`.cgp`)와 에뮬레이터 메모리 import는 지원하지 않으며, 모든 RetroArch 프리셋과 GPU 조합의 동작을 보장하지 않습니다. 자세한 범위는 실행판의 `README_KO_R2.txt`와 `QA_KO_R2.txt`를 확인하세요.

### 다운로드 파일

| 파일 | 내용 |
|---|---|
| `MesenCE_KO_2.2.1_R2_TEST_20261123_01_Windows_x64.zip` | `Mesen.exe`, 필수 DLL, 셰이더, 사용 안내·검증 자료 |
| `MesenCE_KO_2.2.1_R2_TEST_20261123_01_source.zip` | 전체 수정 소스, 빌드 절차, 의존성·라이선스 고지 |
| `MesenCE_KO_2.2.1_R2_TEST_20261123_01_SHA256SUMS.txt` | 위 두 ZIP의 SHA-256 |

원본 ZIP의 내용을 변경하지 않고 게시합니다. 게시 과정에서 원본 해시·ZIP CRC·실행파일 x64 형식을 확인하고, GitHub 첨부 파일을 다시 다운로드해 원본 SHA-256과 대조합니다.

Mesen 본체는 GPL 3 이상이며 전체 수정 소스를 함께 제공합니다. 셰이더별 원저작자와 별도 라이선스 조건은 각 파일 및 동봉 고지를 따릅니다. ROM·BIOS·게임 데이터는 포함하지 않습니다.

원본: [MesenCE 2.2.1](https://github.com/nesdev-org/MesenCE/releases/tag/2.2.1)
