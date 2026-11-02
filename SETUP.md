# 한글패치 아카이브 운영 안내

## 사이트
- 홈페이지: https://bambabo-cmd.github.io/retro-korean-patches/
- 버전 기록: https://bambabo-cmd.github.io/retro-korean-patches/versions.html
- 오류 제보: https://github.com/bambabo-cmd/retro-korean-patches/issues/new/choose

## 공개 다운로드와 버전 관리
패치 파일은 누구나 다운로드할 수 있습니다. 별도 다운로드 인증은 사용하지 않습니다.
기존 한글패치 저장소에는 손대지 않고, 중앙 저장소의 Releases에 버전별 공개 패치를 보존합니다.

1. 각 게임 원본 저장소에 새 패치 파일과 적용/검증 정보를 올립니다.
2. 중앙 저장소 `docs/patches.json`에서 해당 게임 버전·날짜·다운로드 링크를 수정합니다.
3. Actions → 공개 패치 버전 아카이브 → Run workflow 실행.
4. Releases에서 버전 태그·패치 첨부·SHA-256과 시험 배포 표기를 확인합니다.
5. 버전 기록 홈페이지는 중앙 Releases에서 공개된 배포를 읽어 보여줍니다.

릴리스 재실행 시 동일한 태그는 중복 생성하지 않습니다. 검수하지 않은 버전은 정식 완료판으로 표시하지 않습니다.
기본 배포 형식은 원본 ROM에 적용하는 xdelta이며, 실제 배포본이 IPS인 게임은 그 형식을 유지합니다.

## 오류 접수
GitHub 이메일 확인을 마친 계정으로만 Issues를 작성할 수 있습니다. 실명 확인은 아닙니다.
한국어 양식은 실행·프리징/번역·글꼴/개선 제안의 세 종류이며, Open/Closed로 처리 현황을 관리합니다.
문제 재현에 필요한 게임·버전·기기·에뮬레이터 정보를 요청하며 원본 ROM이나 개인정보를 요구하지 않습니다.

## GitHub Pages
Settings → Pages → Deploy from a branch → main /docs

## 파일 관리
원본 게임 ROM은 포함하지 않습니다. 공개 저장소에 비밀번호나 비밀키를 올리지 마세요.
