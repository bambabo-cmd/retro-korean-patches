# Gens KO 2.14 R3 — Real CRT Phosphor와 신규 기능 모듈

이 공개 폴더는 신규 기능 소스, 별도 셰이더 자산과 실제 설정 코드를 실행하는 Windows 검증 도구입니다.
Starscream CPU 코어·전체 Gens_KO.exe·게임 ROM은 포함하지 않습니다.
전체 실행파일의 재배포 약관 문제는 별도로 남아 있습니다.

## Real CRT Phosphor v1.4.3

[공식 v1.4.3](https://github.com/crt2oldgamer-stack/real-crt-phosphor/releases/tag/v1.4.3)을 Gens 전용 GLSL 1.30으로 변환했습니다.
원본 셰이더 15개와 프리셋 36개, 수식·패스 순서·파라미터·상속을 보존했습니다.
14패스, RGBA16F 중간 화면, 잔광 피드백을 처리합니다. 전체 렌더러 상한은 16패스입니다.

Gens KO R3에서 **화면 → 셰이더 / 출력 설정 → Real CRT Phosphor · 기본 → 적용**을 선택합니다.
**세부 설정**에서 마스크·곡률·잔광·밝기를 바꾸고 프리셋별로 저장합니다.
기본 / 4K / Ally 1080p를 선택할 수 있습니다. 곡률·잔광은 원작처럼 처음에는 꺼져 있습니다.
OpenGL 3.0/GLSL 1.30과 실수 프레임버퍼 지원이 필요합니다.
원본 `.slangp`를 직접 실행하는 범용 Slang 지원은 아닙니다.

원작자 **건군(gunkun)**의 별도 비상업 이용 조건을 유지합니다.
이 셰이더는 프로그램 GPL 고지로 재허가되지 않습니다.
원문 LICENSE, 변경 안내와 파일 해시는 `src/shaders/real-crt-phosphor/`에 있습니다.

## 전체 수정 작업의 주요 변경

- 한글 메뉴·설정창, 키보드와 XInput/DirectInput 게임패드 설정.
- ROM 상한 6 → 32MiB, 피어솔라 매퍼·SPI 대응.
- FF·REW·슬로우모션, 기능별 키·패드·홀드/토글·속도 지정.
- GLSL/GLSLP 셰이더, CRT·스캔라인·CRT-Pi와 Real CRT Phosphor.
- 모니터 이동·포커스 전환 출력 경로 개선.
- 게임 바로가기, Alt/Esc 메뉴 표시·숨김, 기존 입력·저장·로딩·CPU 오류 수정.

기본 6버튼: **ABC=CXZ / XYZ=DSA / START=V / MODE=F**, 방향키 이동.
키 설정은 **일괄 설정**을 누르고 안내 순서대로 입력합니다.
**Tab=FF 홀드4배 / Backspace=REW 홀드2배 / Space=슬로우 토글0.5배**입니다.
1배속이 아닌 재생과 REW는 무음이며 메가 CD·넷플레이의 되감기는 지원하지 않습니다.

## 재현과 검증 범위

- `python3 tests/run_public_checks.py`: 입력·핫키·속도·기록·프리셋 및 변환 재현 검사.
- `python3 tests/test_real_crt_phosphor.py`: 원본 수식/UBO와 전용 GLSL 포트를 실제 EGL로 비교.
- `python3 tests/test_renderer_feedback.py`: 실제 renderer 피드백 관리 함수 검사.
- `python3 build_shader_probe.py`: MinGW 32비트 C++/windres로 설정·렌더러 구성 요소 빌드.
- Windows에서 `tests/windows_smoke.ps1 -SettingsProbe -IncludeShaderSettings -RealCrt -ExeDirectory <도구폴더> -OutputDirectory settings-proof`로 실제 WGL 출력과 설정 화면 확인.

CI는 해시가 고정된 Mesa 소프트웨어 OpenGL을 별도 임시 폴더에서 사용합니다.
검증 DLL은 배포물에 넣지 않습니다. 실제 게임패드·다중 모니터·전체 게임 플레이의 확인과 다릅니다.
통과 여부와 실제 캡처는 Actions 결과 및 릴리스 증거 ZIP을 확인하세요.

라이선스 범위는 [PUBLIC_SCOPE_KO.txt](PUBLIC_SCOPE_KO.txt), 게시판 안내는 [FORUM_POST_KO.txt](FORUM_POST_KO.txt)에 있습니다.
