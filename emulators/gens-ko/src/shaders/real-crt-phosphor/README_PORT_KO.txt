Real CRT Phosphor v1.4.3 — Gens KO 전용 GLSL 포트

원작자: 건군(gunkun). 기획·화면 방향·실기 테스트·프리셋 조정은 원작자의 작업입니다.
원본: https://github.com/crt2oldgamer-stack/real-crt-phosphor/releases/tag/v1.4.3
원본 코드의 AI 저작물 표시는 원본 LICENSE.txt / LICENSE_KR.txt에 그대로 보존했습니다.

이 폴더는 원본 Slang 셰이더를 Gens KO의 OpenGL 인터페이스에 맞춘 전용 포트입니다.
범용 Slang 로더가 아니며 .slangp 파일을 직접 실행하는 기능도 아닙니다.
원본 14패스, GBA CRT 15패스, 단일 패스 GBA 색 보정과 원본 수식·상수를 보존합니다.
GLSL 1.30으로 단계/입력 선언을 바꾸고, float 중간 버퍼와 잔광 피드백을 사용합니다.
모든 #reference 상속은 상대 경로와 설정 우선순위를 유지하여 .glslp에 평탄화했습니다.

사용: Gens KO의 화면 출력 및 셰이더 설정에서 이 폴더의
real-crt-phosphor.glslp 또는 presets/4K, presets/Ally 아래의 .glslp를 불러옵니다.
기본 프리셋의 AFTERGLOW(잔상)와 CURVATURE(곡률)는 원본처럼 0입니다.
프리셋을 적용한 뒤 '세부 설정'에서 숫자를 바꾸고 '적용·저장'을 누르세요.
설정값은 Gens-KO-shaders.ini에 프리셋별로 보관합니다.
OpenGL 3.0/GLSL 1.30 이상 및 실수형 프레임버퍼 지원이 필요합니다.
GDI 호환 출력에서는 셰이더를 적용할 수 없습니다.

이 셰이더는 Gens KO 프로그램의 GPL 라이선스로 재허가되지 않습니다.
원작자의 별도 비상업적 이용 조건을 그대로 따릅니다. 원문 LICENSE 두 파일을 확인하세요.
재배포 시 원작자와 Real CRT Phosphor 이름, 원문 이용 조건, 이 포팅 고지를 보존하세요.
수정일: 2026-11-20. 변환 소스: port_real_crt_phosphor.py.
SOURCE.json에는 원본 ZIP 및 각 원본/포트 파일의 SHA-256을 기록했습니다.
원본 README_EN.txt / README_KR.txt는 원작자의 Slang 설치 안내이므로,
Gens KO에서는 위 .glslp 사용 방법을 따르세요.
