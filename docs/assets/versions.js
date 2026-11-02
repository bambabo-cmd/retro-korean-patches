'use strict';
const select=document.getElementById('game-select'),root=document.getElementById('history-content');
const params=new URLSearchParams(location.search);
function el(tag,cls,text){const node=document.createElement(tag);if(cls)node.className=cls;if(text!==undefined)node.textContent=String(text);return node}
function ghUrl(s){try{const u=new URL(s);return u.protocol==='https:'&&u.hostname==='github.com'}catch{return false}}
function a(label,url,cls){if(!ghUrl(url))return el('span','',label);const x=el('a',cls||'',label);x.href=url;x.target='_blank';x.rel='noopener noreferrer';return x}
function normalize(s){return String(s).replace(/[^a-z0-9._-]+/gi,'-').replace(/^-+|-+$/g,'')}
Promise.all([
 fetch('patches.json',{cache:'no-store'}).then(r=>{if(!r.ok)throw Error('목록');return r.json()}),
 fetch('history.json',{cache:'no-store'}).then(r=>{if(!r.ok)throw Error('기록');return r.json()})
]).then(async ([patches,history])=>{
 let releases=[];let online=true;
 try{
  const r=await fetch('https://api.github.com/repos/bambabo-cmd/retro-korean-patches/releases?per_page=100',{headers:{Accept:'application/vnd.github+json'}});
  if(!r.ok)throw Error('GitHub API');
  releases=await r.json();
 }catch{online=false}
 for(const p of patches){const n=p.repository_url.split('/').pop(),opt=el('option','',p.platform+' · '+p.title);opt.value=n;select.append(opt)}
 const want=params.get('game')||'';
 if([...select.options].some(x=>x.value===want))select.value=want;
 function render(){
  const fragment=document.createDocumentFragment();
  const entries=patches.filter(p=>!select.value||p.repository_url.endsWith('/'+select.value));
  for(const p of entries){
   const slug=p.repository_url.split('/').pop();
   const card=el('section','history-group'),title=el('div','history-title');
   title.append(el('h2','',p.title),el('span','',p.platform));
   card.append(title);
   const all=[...(history[slug]||[])];
   for(const r of releases){
    if(typeof r.tag_name!=='string'||!r.tag_name.startsWith('archive/'+slug+'/')||r.draft)continue;
    all.unshift({source:'release',version:r.tag_name.split('/').slice(2).join('/'),date:(r.published_at||'').slice(0,10),status:r.prerelease?'시험판':'정식 릴리스',url:r.html_url});
   }
   const seen=new Set();const rows=[];
   for(const row of all){
    const key=normalize(row.version);
    if(seen.has(key))continue;
    seen.add(key);rows.push(row);
   }
   for(const row of rows){
    const item=el('article','history-row');
    const left=el('div','history-detail');left.append(el('strong','',row.version),el('small','',row.source==='release'?'GitHub Release':row.source==='file'?'이전 패치 파일':'README 기록'));
    const right=el('div','history-end');right.append(el('span','',row.date||'날짜 미확인'));
    if(ghUrl(row.url))right.append(a(row.source==='release'?'릴리스 보기 ↗':row.source==='file'?'파일 받기 ↗':'저장소 보기 ↗',row.url,'history-link'));
    item.append(left,right);
    card.append(item);
   }
   if(rows.length===0)card.append(el('p','muted','등록된 버전 기록이 없습니다.'));
   const links=el('div','history-actions');
   for(const d of (p.downloads||[])){if(ghUrl(d.url))links.append(a(d.label+' 패치 ↓',d.url,'history-button'))}
   links.append(a('원본·적용 안내 ↗',p.repository_url,'history-button'));
   card.append(links);
   fragment.append(card);
  }
  root.replaceChildren(fragment);
 }
 if(!online){const note=el('p','history-api-note','GitHub Releases 실시간 조회가 제한되어 README와 저장된 기록만 표시합니다.');root.before(note)}
 select.addEventListener('change',()=>{const u=new URL(location.href);if(select.value)u.searchParams.set('game',select.value);else u.searchParams.delete('game');window.history.replaceState(null,'',u);render()});
 render();
}).catch(()=>{root.textContent='버전 기록을 읽지 못했습니다. 다시 접속해 주세요.'});
