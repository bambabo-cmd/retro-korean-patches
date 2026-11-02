/**
 * Cloudflare Worker: verified GitHub account -> private R2 patch download.
 * All access checks occur on the Worker. Public GitHub Pages only links to /download/:id.
 * Docs: download-gateway/README.md
 */
const SESSION_COOKIE = '__Host-rh_session';
const OAUTH_COOKIE = '__Host-rh_oauth';
const OAUTH_TTL_SECONDS = 600;
const SESSION_TTL_SECONDS = 12 * 60 * 60;
const enc = new TextEncoder();
const dec = new TextDecoder();
function error(status, message) {
  return new Response(message, { status, headers: {
    'Content-Type': 'text/plain; charset=utf-8',
    'Cache-Control': 'no-store',
    'X-Content-Type-Options': 'nosniff'
  }});
}
function redirect(url, cookies = []) {
  const h = new Headers({'Location': url, 'Cache-Control': 'no-store', 'Referrer-Policy': 'no-referrer'});
  for (const c of cookies) h.append('Set-Cookie', c);
  return new Response(null, {status:302,headers:h});
}
function cookie(name,value,maxAge) {
  return name+'='+value+'; Path=/; Max-Age='+maxAge+'; HttpOnly; Secure; SameSite=Lax';
}
function getCookie(request,name) {
  const line = request.headers.get('Cookie') || '';
  const found = line.split(';').map(c=>c.trim()).find(c=>c.startsWith(name+'='));
  return found ? found.slice(name.length+1) : null;
}
function b64url(bytes) {
  let s='';for(const b of bytes)s+=String.fromCharCode(b);
  return btoa(s).replace(/\+/g,'-').replace(/\//g,'_').replace(/=+$/,'');
}
function unb64url(value) {
  if (!/^[A-Za-z0-9_-]+$/.test(value)) throw new Error('Invalid encoding');
  const plain = value.replace(/-/g,'+').replace(/_/g,'/');
  const padded = plain.padEnd(Math.ceil(plain.length/4)*4,'=');
  return Uint8Array.from(atob(padded),c=>c.charCodeAt(0));
}
async function hmacKey(secret) {
  return crypto.subtle.importKey('raw',enc.encode(secret),{name:'HMAC',hash:'SHA-256'},false,['sign','verify']);
}
async function sign(payload,secret) {
  const body=b64url(enc.encode(JSON.stringify(payload)));
  const key=await hmacKey(secret);
  const signature=new Uint8Array(await crypto.subtle.sign('HMAC',key,enc.encode(body)));
  return body+'.'+b64url(signature);
}
async function verify(token,secret) {
  if(!token||typeof token!=='string'||token.length>3500)return null;
  const parts=token.split('.');if(parts.length!==2)return null;
  try {
    const key=await hmacKey(secret);
    const valid=await crypto.subtle.verify('HMAC',key,unb64url(parts[1]),enc.encode(parts[0]));
    if(!valid)return null;
    const value=JSON.parse(dec.decode(unb64url(parts[0])));
    if(typeof value.exp!=='number'||value.exp<=Math.floor(Date.now()/1000))return null;
    return value;
  }catch{return null;}
}
function uniqueId(){return b64url(crypto.getRandomValues(new Uint8Array(28)));}
function safeId(value){return /^[A-Za-z0-9][A-Za-z0-9._-]{0,99}$/.test(value||'');}
function manifest(env){
  try {
    const obj=JSON.parse(env.DOWNLOADS_JSON||'{}');
    if (!obj || typeof obj!=='object' || Array.isArray(obj)) return {};
    return obj;
  } catch { return {}; }
}
function getEntry(env,id){
  if(!safeId(id))return null;
  const data=manifest(env);
  if(!Object.prototype.hasOwnProperty.call(data,id))return null;
  const entry=data[id];
  if(!entry || typeof entry!=='object'||typeof entry.key!=='string')return null;
  if(!/^[A-Za-z0-9][A-Za-z0-9._/-]{0,255}$/.test(entry.key)||entry.key.includes('..'))return null;
  return entry;
}
function canAccessUser(env,userId){
  const mode=(env.ACCESS_MODE||'verified').toLowerCase();
  if(mode==='verified')return true;
  if(mode==='approved')return (env.APPROVED_GITHUB_IDS||'').split(',').map(x=>x.trim()).includes(String(userId));
  return false;
}
function requiredSettings(env) {
  return Boolean(env.GITHUB_CLIENT_ID && env.GITHUB_CLIENT_SECRET && env.GITHUB_CALLBACK_URL &&
    env.SESSION_SECRET && env.SESSION_SECRET.length>=32 && env.PATCH_BUCKET &&
    ['verified','approved'].includes((env.ACCESS_MODE||'verified').toLowerCase()));
}
async function githubJson(url,token) {
  const res=await fetch(url,{headers:{'Accept':'application/vnd.github+json','Authorization':'Bearer '+token,'User-Agent':'RetroHangul-PatchArchive','X-GitHub-Api-Version':'2022-11-28'},redirect:'error'});
  if(!res.ok)throw Error('GitHub 사용자 확인 실패');
  return res.json();
}
async function login(request,env) {
  if(!requiredSettings(env))return error(503,'인증 서버 설정이 완료되지 않았습니다.');
  const url=new URL(request.url);
  const next=url.searchParams.get('next')||'';
  if(!/^\/download\/[A-Za-z0-9][A-Za-z0-9._-]{0,99}$/.test(next))return error(400,'잘못된 다운로드 경로입니다.');
  const id=next.slice('/download/'.length);
  if(!getEntry(env,id))return error(404,'등록되지 않은 패치입니다.');
  const state=uniqueId();
  const signed=await sign({state,next,exp:Math.floor(Date.now()/1000)+OAUTH_TTL_SECONDS},env.SESSION_SECRET);
  const oauth=new URL('https://github.com/login/oauth/authorize');
  oauth.searchParams.set('client_id',env.GITHUB_CLIENT_ID);
  oauth.searchParams.set('redirect_uri',env.GITHUB_CALLBACK_URL);
  oauth.searchParams.set('scope','user:email');
  oauth.searchParams.set('state',state);
  return redirect(oauth.href,[cookie(OAUTH_COOKIE,signed,OAUTH_TTL_SECONDS)]);
}
async function callback(request,env) {
  if(!requiredSettings(env))return error(503,'인증 서버 설정이 완료되지 않았습니다.');
  const url=new URL(request.url);
  const code=url.searchParams.get('code');
  const state=url.searchParams.get('state');
  const oauth=await verify(getCookie(request,OAUTH_COOKIE),env.SESSION_SECRET);
  if(!oauth||!state||oauth.state!==state||!code||!/^\/download\/[A-Za-z0-9][A-Za-z0-9._-]{0,99}$/.test(oauth.next))return error(403,'인증 요청이 유효하지 않습니다. 처음부터 다시 로그인하세요.');
  const tokenRes=await fetch('https://github.com/login/oauth/access_token',{
    method:'POST',headers:{'Accept':'application/json','Content-Type':'application/json','User-Agent':'RetroHangul-PatchArchive'},
    body:JSON.stringify({client_id:env.GITHUB_CLIENT_ID,client_secret:env.GITHUB_CLIENT_SECRET,code,redirect_uri:env.GITHUB_CALLBACK_URL}),
    redirect:'error'
  });
  if(!tokenRes.ok)return error(502,'GitHub 인증 통신에 실패했습니다.');
  const tokenJson=await tokenRes.json();
  if(!tokenJson.access_token)return error(403,'GitHub 로그인을 완료하지 못했습니다.');
  let user,emails;
  try {
    [user,emails]=await Promise.all([
      githubJson('https://api.github.com/user',tokenJson.access_token),
      githubJson('https://api.github.com/user/emails?per_page=100',tokenJson.access_token)
    ]);
  }catch{return error(502,'GitHub 계정 정보를 확인하지 못했습니다.');}
  if(!Number.isInteger(user?.id)||!Array.isArray(emails)||!emails.some(e=>e.primary===true&&e.verified===true))return error(403,'확인된 기본 이메일이 있는 GitHub 계정만 이용할 수 있습니다.');
  if(!canAccessUser(env,user.id))return error(403,'이 계정은 다운로드 승인 대상이 아닙니다.');
  const signed=await sign({uid:user.id,login:String(user.login||''),emailVerified:true,exp:Math.floor(Date.now()/1000)+SESSION_TTL_SECONDS},env.SESSION_SECRET);
  const origin=new URL(request.url).origin;
  return redirect(origin+oauth.next,[cookie(OAUTH_COOKIE,'',0),cookie(SESSION_COOKIE,signed,SESSION_TTL_SECONDS)]);
}
function safeDownloadFilename(entry,id){
  const name=String(entry.filename||id+'.xdelta');
  return name.replace(/[\r\n"\\/\u0000-\u001f\u007f]/g,'_').slice(0,180);
}
async function download(request,env,id) {
  if(!requiredSettings(env))return error(503,'다운로드 서버 설정이 완료되지 않았습니다.');
  const entry=getEntry(env,id);
  if(!entry)return error(404,'등록되지 않은 배포 파일입니다.');
  const session=await verify(getCookie(request,SESSION_COOKIE),env.SESSION_SECRET);
  if(!session||!session.emailVerified||!Number.isInteger(session.uid)||!canAccessUser(env,session.uid)){
    const origin=new URL(request.url).origin;
    return redirect(origin+'/auth/login?next='+encodeURIComponent('/download/'+id));
  }
  let obj;
  try {obj=await env.PATCH_BUCKET.get(entry.key);} catch{return error(503,'저장소에서 파일을 읽지 못했습니다.');}
  if(!obj)return error(404,'아직 업로드되지 않은 파일입니다.');
  const name=safeDownloadFilename(entry,id);
  const encoded=encodeURIComponent(name);
  const headers=new Headers({
    'Content-Type':'application/octet-stream',
    'Content-Disposition':"attachment; filename=\"patch.xdelta\"; filename*=UTF-8''"+encoded,
    'Cache-Control':'private, no-store',
    'X-Content-Type-Options':'nosniff',
    'Referrer-Policy':'no-referrer'
  });
  if(Number.isFinite(obj.size))headers.set('Content-Length',String(obj.size));
  return new Response(obj.body,{status:200,headers});
}
export default {
  async fetch(request,env) {
    const url=new URL(request.url);
    if(request.method!=='GET')return error(405,'GET 요청만 지원합니다.');
    if(url.pathname==='/health')return new Response(JSON.stringify({ok:true,configured:requiredSettings(env)}),{headers:{'Content-Type':'application/json; charset=utf-8','Cache-Control':'no-store'}});
    if(url.pathname==='/auth/login')return login(request,env);
    if(url.pathname==='/auth/callback')return callback(request,env);
    const found=url.pathname.match(/^\/download\/([A-Za-z0-9._-]+)$/);
    if(found)return download(request,env,found[1]);
    return error(404,'경로를 찾을 수 없습니다.');
  }
};
