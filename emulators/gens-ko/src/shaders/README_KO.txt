Gens KO 화면 출력과 셰이더

화면 출력
- 창 전용 OpenGL 컨텍스트를 사용하며 바탕 화면의 색 깊이나 해상도를 바꾸지 않습니다.
- 마지막 완성 프레임을 CPU에 보관하여 일시정지·다른 창 사용·창 다시 그리기 때 유지합니다.
- 모니터가 바뀌면 창의 컨텍스트를 다시 만들고 보관한 화면을 복원합니다.
- OpenGL을 사용할 수 없거나 출력이 실패하면 GDI 호환 출력으로 돌아갑니다.
- GDI는 셰이더 효과를 적용하지 않습니다.

메뉴의 '화면 출력 및 셰이더'에서 기본 효과를 선택하거나 외부 .glsl/.glslp를 지정합니다.
프리셋과 참조하는 .glsl 파일의 폴더 구조를 그대로 유지해 주세요.

지원 범위
- OpenGL 2.0 이상, 데스크톱 GLSL의 VERTEX/FRAGMENT 조건부 형식
- GLSL 파일 단독 또는 GLSLP 프리셋, 최대 8패스(실제 GPU 텍스처 슬롯 한도 적용)
- source/viewport/absolute 배율, 가로·세로 개별 배율
- nearest/linear 필터, clamp_to_edge/clamp_to_border/repeat/mirrored_repeat
- Texture/InputSize/TextureSize/OutputSize, FrameCount/FrameDirection, MVPMatrix
- OrigTexture, 앞선 PassNTexture 및 PassPrevNTexture, pass alias
- #pragma parameter 기본값과 프리셋의 parameters 목록/값
- 상대 경로 #include를 사용하는 GLSL 소스(최대 8단계)
- shader framebuffer 총 32M 픽셀, 각 변 최대 8192 또는 GPU 한도

지원하지 않는 기능은 오류로 안내하고 기존 설정을 유지합니다.
- .slang/.slangp, .cg/.cgp
- LUT 외부 텍스처, 이전 프레임(Prev), 피드백, float/sRGB framebuffer, mipmap
- GLSLP #reference/#include 상속 프리셋

기본 효과
- sharp: 선명한 기본 픽셀
- smooth: 선형 보간
- scanlines: 약한 가로 스캔라인
- crt: 가벼운 화면 곡률·RGB 마스크·스캔라인
- two-pass: 2배 픽셀 확대한 뒤 스캔라인을 적용하는 2패스 예제
- retroarch-crt-pi: Libretro 배포 CRT-Pi 셰이더(davej 작성, GPL v2 이상)

Gens-KO-video.ini: 화면/셰이더 설정
Gens-KO-renderer.log: 출력 장치 및 셰이더 오류 기록

기존 Gens의 소프트웨어 필터도 유지됩니다. 셰이더와 동시에 켜면 소프트웨어
필터 결과가 셰이더의 입력이 되므로, 원래 픽셀에 효과를 적용하려면 기존
렌더 모드를 '일반' 또는 '2배'로 설정하세요.

직접 작성한 기본 셰이더는 GPL v2로 배포합니다.
CRT-Pi는 원저자 davej의 저작권과 GPL v2 이상 고지를 유지했습니다.
외부에서 가져온 셰이더를 다시 배포하려면 해당 파일의 라이선스를 별도로 확인하세요.
