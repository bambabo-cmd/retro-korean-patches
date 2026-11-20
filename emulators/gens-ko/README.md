# Gens KO 2.14 R2 — 신규 기능 코드와 설정 UI 자료

이 공개 폴더는 **전체 에뮬레이터가 아닙니다.** 신규 기능 모듈과 실제 제품의 설정 코드를 별도로 실행하는 Windows 검증 도구입니다.
원본 Starscream CPU 코어, 완성형 Gens_KO.exe와 게임 ROM은 포함하지 않습니다. 전체 실행파일의 공개 배포는 구성요소 약관 문제가 해결된 것으로 표시하지 않습니다.

## 설정 화면 재현

Linux에서 `python3 build_shader_probe.py`로 코어가 없는 설정 도구를 만들 수 있습니다. MinGW 32비트 C++와 windres가 필요합니다.
Windows에서 `tests/windows_smoke.ps1 -SettingsProbe -IncludeShaderSettings -ExeDirectory build/shader_probe -OutputDirectory settings-proof`를 실행하면 실제 키보드·패드, 핫키·속도, 셰이더 설정 창을 캡처합니다.
게임 실행이 아닌 설정 구성 요소 검사입니다. FF/REW의 게임 동작이나 피어솔라 호환성 증거로 사용하지 않습니다.

핫키·입력·타이밍·압축 기록 논리 검사는 `python3 tests/run_public_checks.py`로 재현합니다.
라이선스와 제외 범위는 [PUBLIC_SCOPE_KO.txt](PUBLIC_SCOPE_KO.txt), 짧은 게시글은 [FORUM_POST_KO.txt](FORUM_POST_KO.txt)를 확인하세요.

## 전체 수정 작업 안내


Windows Gens 2.14를 바탕으로 한 비공식 한국어 수정본입니다.
R1의 화면·입력 개선에 FF, REW, 슬로우모션과 개별 홀드·토글 설정을 추가했습니다.

## 기본 조작

| 기능 | 기본키 | 방식 / 속도 |
|---|---|---|
| 빨리 감기 | Tab | 홀드 / 4배속 |
| 되감기 | Backspace | 홀드 / 2배속 |
| 슬로우모션 | Space | 토글 / 0.5배속 |
| 메뉴 표시·숨기기 | Alt 또는 Esc | 토글 |
| 일시정지 | Pause | 토글 |
| 리셋 | Ctrl+R | 한 번 누르기 |

각 재생 기능의 키보드·패드 입력, 홀드·토글, 속도는 **설정 → 핫키 / 배속 / 되감기 설정**에서 바꿉니다.

## 게임 버튼

새 설정은 6버튼 패드가 기본입니다.

| 메가드라이브 버튼 | A | B | C | X | Y | Z | START | MODE |
|---|---|---|---|---|---|---|---|---|
| 키보드 | C | X | Z | D | S | A | V | F |

방향은 방향키입니다. **설정 → 키보드 / 게임패드 → 일괄 설정**을 누르고 화면에서 요구하는 순서대로 입력합니다.
사용자가 바꾼 이전 배치는 유지하며, 수정하지 않은 R1의 기본 1P 배치만 새 기본값으로 이관합니다.

## 주요 변경

- 한글 메뉴·설정창, 키보드와 XInput/DirectInput 패드의 개별 매핑.
- FF·REW·슬로우모션, 기본 재생 속도, 기능별 홀드·토글 설정 저장.
- MD·피어솔라·32X의 CPU·RAM·매퍼·음원·화면 상태를 함께 복원하는 되감기.
- 기본 되감기 기록 30초·128MiB, 시간·메모리 상한과 기록 사용 여부 설정.
- ROM 처리 상한 6MiB → 32MiB, 피어솔라 매퍼/SPI 대응.
- 창 전환·모니터 이동 경로 개선, 테두리 없는 전체 화면.
- GLSL/GLSLP 셰이더, ROM 인자·드래그 실행, 게임 바로가기 생성.

## 현재 한계

1배속 이외의 재생 및 REW는 무음입니다. REW는 메가 CD·넷플레이·WAV/GYM 기록에서 지원하지 않습니다.
Windows 실기, 물리 패드, 다중 모니터 및 전체 게임 플레이 검증은 완료로 표시하지 않습니다.
검사 범위와 결과는 `VALIDATION.json`, 재현 방법은 `BUILD_KO.txt`를 참고하세요.

## 라이선스와 공개 범위

Gens 원본에는 GNU GPL 안내와 GPL v2 전문이 들어 있지만, 내장 Starscream 코어에는 원형 배포 및 비상업 조건이 남아 있습니다.
이번 수정본에 적용되는 별도 재허가나 예외 허락은 확인하지 못했습니다. 무료 배포나 소스 공개만으로 이 충돌이 해결되는 것은 아닙니다.

전체 실행파일의 공개 재배포가 허용됐다고 선언하지 않습니다. 원저자 고지와 `docs/LICENSE_REVIEW_KO.txt`를 보존합니다.
별도로 공개하는 자체 작성 기능 모듈과 전체 에뮬레이터의 배포 권한은 구분합니다.
게임 ROM과 외부 BIOS 덤프는 에뮬레이터 배포물에 포함하지 않습니다.

근거: [GNU GPL v2](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html),
[Debian의 Starscream 약관 검토 기록](https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=533715),
원본 `main68kgen.txt`·`sub68kgen.txt`의 Terms of Use.
