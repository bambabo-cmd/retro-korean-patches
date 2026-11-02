# 홈페이지 설정

1. GitHub 저장소 Settings → Pages → Deploy from a branch → main /docs → Save.
2. Settings → General → Features → Issues 활성화.
3. 공개 후 https://bambabo-cmd.github.io/retro-korean-patches/ 확인.
4. 새 프로젝트 등록 시 docs/patches.json 수정 및 GitHub 저장소 연결.
5. 새 버전별 패치는 각 게임 저장소 Releases에 태그와 검증 정보를 함께 등록.

## 인증 다운로드
현재 설치된 페이지는 공개 저장소 링크를 표시합니다. GitHub 로그인으로 *진짜 다운로드 제한*을 적용하려면 별도의 GitHub OAuth 서버와 비공개 파일 저장소가 필요합니다. 별도 구성 전에는 공개 파일의 접근 제한이 불가능합니다. 인증 서버 배포는 이 저장소의 Pages 활성화와 다른 작업입니다.

## 보안
원본 ROM, OAuth Client Secret, 세션 서명키는 이 공개 저장소에 업로드하면 안 됩니다.
