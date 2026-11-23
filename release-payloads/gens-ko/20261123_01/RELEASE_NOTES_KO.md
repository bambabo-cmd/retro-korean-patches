# Gens KO 2.14 R6 — RetroArch GLSL·Slang 셰이더 지원

**Windows 실행 파일과 대응하는 전체 소스를 함께 제공하는 시험 배포본입니다.**

## 다운로드

- **일반 사용자:** `Gens_KO_R6_20261123_01_Windows.zip`
- **전체 소스·빌드 방법·상세 검증 기록:** `Gens_KO_R6_20261123_01_Source.zip`
- **검증 결과 요약:** `Gens_KO_R6_20261123_01_Validation_KO.txt`
- **첨부 파일 SHA-256:** `Gens_KO_R6_20261123_01_SHA256SUMS.txt`

Windows ZIP 전체를 풀고 `Gens_KO/Gens_KO.exe`를 실행하세요. `librashader.dll`과 `shaders` 폴더를 실행 파일 옆에 함께 두어야 합니다. Windows 10·11 사용 대상인 32비트 실행 파일입니다.

## 이번 버전의 변경점

- 요청한 **CRT Mattias 원본 셰이더**와 실행용 프리셋을 포함했습니다. `화면 → 셰이더 / 출력 설정 → CRT Mattias → 적용`으로 선택합니다.
- RetroArch의 **GLSL 및 Slang 셰이더 폴더를 복사해 사용**할 수 있습니다. 하위 폴더까지 검색하고, 외부 파일 선택도 지원합니다.
- `.glsl` / `.glslp`: 최대 64패스, 매개변수, LUT 이미지, include/reference, mipmap, sRGB/실수 버퍼, 이전 프레임 및 잔광을 처리합니다.
- `.slang` / `.slangp`: 동봉한 **librashader 0.12.1** 기반 OpenGL 런타임으로 다중 패스, LUT, reference, 매개변수와 이력·잔광을 처리합니다.
- Slang 입력의 mipmap 미완성으로 화면이 검게 나오는 문제와 첫 프레임 이력에 이전 텍스처 색상이 섞이는 문제를 수정했습니다.
- R5의 메가 CD CDC 버퍼 수정, BIN+CUE/CDDA 처리, BIOS 자동 검색과 기존 입력·저장 기능을 유지합니다.

## RetroArch 셰이더 사용법

1. RetroArch의 `shaders_glsl` 또는 `shaders_slang` 폴더를 Gens의 `shaders` 아래에 복사합니다.
2. **하위 폴더, include, 참조 프리셋과 LUT 이미지도 함께 유지**합니다.
3. 셰이더 설정에서 새로 고침을 누르고 사용할 `.glslp` 또는 `.slangp` 프리셋을 선택합니다.

예: `Gens_KO/shaders/shaders_slang/crt/…`

Slang은 OpenGL 3.3 이상과 texture storage 기능이 필요합니다. OpenGL 4.2 이상을 지원하는 그래픽 드라이버가 이 조건을 충족합니다. 효과별 GPU 요구 조건도 적용됩니다. **Cg(`.cg`/`.cgp`)와 HDR 최종 출력은 지원하지 않으며, 모든 RetroArch 프리셋의 호환성을 보장하지 않습니다.**

## 검증한 범위

- GLSL 프리셋 파서 2,361개 형식 검사 및 기본 프리셋 43개.
- 실제 GLSL OpenGL 출력 22개 시나리오·2,250개 확인 항목.
- 셰이더 경로 처리 123개 검사 및 동일한 ASan/UBSan 검사, GLSL 잔광 상태 174개, Slang 출력 캐시 183개 확인 항목.
- 실제 Slang C API/OpenGL 출력 9개 시나리오. 기준 픽셀이 있는 8개 시나리오·16프레임은 채널 오차 0이며, 공식 Real CRT Phosphor 원본 14패스는 3프레임 정상 출력을 확인했습니다.
- Windows EXE/DLL의 x86 PE32 형식, 필요한 C API export, 외부 컴파일러 런타임 DLL 의존성 없음, 검증한 네이티브 라이브러리와 Windows DLL의 소스·패치 일치.
- 실행 ZIP 439개 파일, 소스 ZIP 1,544개 파일 및 소스 명세의 모든 SHA-256 일치.

GPU 실행 검사는 **Linux Mesa EGL**에서 수행했습니다. **Windows Gens 창·WGL·실제 사용자 그래픽카드에서의 직접 실행은 이 환경에서 확인하지 못했습니다.** 자세한 결과와 재현용 테스트는 소스 ZIP의 `VALIDATION.json`, `verification/r6_shaders/` 및 `tests/`에 있습니다.

## 포함 파일 및 고지

사용자 설정·세이브·게임 ROM·별도 BIOS는 포함하지 않았습니다. 메가 CD BIOS는 실행 파일 옆 `bios` 폴더에 두거나 기존 설정 창에서 지정하세요. BIN+CUE 게임은 트랙을 함께 둔 상태에서 CUE를 엽니다.

기존 라이선스와 저작권 고지, 사용한 셰이더 및 의존성 고지를 보존했습니다. 전체 소스 ZIP에는 빌드 설명, librashader 로컬 패치와 대응 소스, 필요한 의존성 고지를 포함했습니다. 배포물의 `LICENSE.txt`, `THIRD_PARTY_NOTICES.txt` 및 `licenses/`를 확인하세요.

## SHA-256

```text
4a15cebdb0925a5bc8aaca1346d7ecfa729c2fe6260c535d47e8ef6da7648d4a  Gens_KO_R6_20261123_01_Windows.zip
6f13ba9592983df5e770254cc8d0a3dc15f2c6a124c2491e23cbbd526ed69012  Gens_KO_R6_20261123_01_Source.zip
5b6be9b54e9106652e46325d7605eb41b1c44c810d33461b38ff5723ccdd3a3e  Gens_KO_R6_20261123_01_Validation_KO.txt
```
