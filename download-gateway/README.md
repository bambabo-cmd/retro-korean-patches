# GitHub OAuth 인증 다운로드 서버

이 폴더는 별도로 배포하는 Cloudflare Worker + 비공개 R2 예제입니다. **현재 실제 배포된 서비스가 아니며, 기존 GitHub 공개 패치 접근도 제한하지 않습니다.**

## 설치

1. Cloudflare R2에서 비공개 버킷 `retro-hangul-private` 생성. 퍼블릭 액세스는 끕니다.
2. `wrangler.toml`의 `GITHUB_CLIENT_ID`, `GITHUB_CALLBACK_URL` 플레이스홀더를 실제 값으로 변경합니다.
3. PC에서 Node.js 설치 후 이 폴더에서 `npx wrangler login`, `npx wrangler deploy` 실행합니다.
4. GitHub Developer settings → OAuth Apps에서 새 OAuth App을 생성하고, Worker URL의 `/auth/callback`을 Callback URL로 등록합니다.
5. 실제 Client Secret과 길이 32자 이상의 무작위 세션키를 **비밀키로만** 등록합니다.

```powershell
npx wrangler secret put GITHUB_CLIENT_SECRET
npx wrangler secret put SESSION_SECRET
npx wrangler deploy
```

6. R2에 ROM이 아닌 xdelta/IPS 패치만 업로드합니다.
7. `DOWNLOADS_JSON`에 안전한 다운로드 ID → 객체 키를 등록하고 다시 배포합니다.
8. `docs/site-config.json`에서 `download_gateway_base`를 실제 Worker HTTPS 주소로 입력한 후, `docs/patches.json`에서 새 패치의 `access`와 `download_id`를 등록합니다.
9. 로그인하지 않은 브라우저, 이메일 미확인 계정, 허용되지 않은 계정으로 차단 여부를 확인합니다.

### 접근 제어
- `ACCESS_MODE="verified"`: 확인된 기본 이메일이 있는 GitHub 계정.
- `ACCESS_MODE="approved"`: `APPROVED_GITHUB_IDS`에 있는 숫자 계정 ID만.

### 유의
- 확인된 이메일 ≠ 실명 확인. 받은 패치의 재배포를 완전히 막을 수도 없습니다.
- 비밀키, OAuth Access Token, 원본 ROM을 GitHub 공개 저장소에 올리지 마세요.
- 공개 GitHub Pages UI에서 버튼만 숨기는 것은 접근 통제가 아닙니다.
- 실서비스에는 rate limiting, 로그, 키 회전 및 장기 운영 점검이 추가로 필요할 수 있습니다.
