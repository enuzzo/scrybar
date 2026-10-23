// Browser-only QA. It never requests a serial port or installs firmware.
// Provide an existing Playwright runtime; do not install dependencies in Dropbox.
import {createRequire} from 'node:module';
import {mkdirSync,writeFileSync} from 'node:fs';
import {resolve,join} from 'node:path';
const require=createRequire(import.meta.url);
const {chromium,webkit}=require(process.env.SCRYBAR_PLAYWRIGHT_MODULE||'playwright');
const args=process.argv.slice(2);
const option=(key,fallback)=>{const i=args.indexOf(key);return i<0?fallback:args[i+1];};
const url=option('--url','http://127.0.0.1:5178/');
const out=resolve(option('--out','/tmp/scrybar-web-qa'));
mkdirSync(out,{recursive:true});
const engine=option('--engine','chromium');
const widths=option('--widths','320,375,390,430,768,1440').split(',').map(Number);
const browser=engine==='webkit'?await webkit.launch({headless:true}):await chromium.launch({channel:'chrome',headless:true});
const report={url,engine,at:new Date().toISOString(),notes:'Browser engine responsive checks, not physical-phone acceptance. Installer success/failure uses a loader fixture. No USB or backend persistence checked.',checks:[]};
const check=(name,pass,detail={})=>report.checks.push({name,pass,...detail});
const stub="customElements.define('esp-web-install-button',class extends HTMLElement{connectedCallback(){this.attachShadow({mode:'open'}).innerHTML='<slot name=activate></slot>';}});";
async function pageAt(width,mode='success'){
 const page=await browser.newPage({viewport:{width,height:width<600?844:900}});
 page.setDefaultTimeout(10000);
 const errors=[];page.on('pageerror',e=>errors.push(e.message));
 let requests=0;
 await page.route('https://unpkg.com/esp-web-tools@*/**',async route=>{
  requests++;
  if(mode==='failure'||(mode==='recover'&&requests===1))return route.abort('failed');
  return route.fulfill({status:200,contentType:'text/javascript',body:stub});
 });
 if(mode==='unsupported')await page.addInitScript(()=>Object.defineProperty(navigator,'serial',{value:undefined,configurable:true}));
 if(mode==='unsupported')await page.addInitScript(()=>{delete Object.getPrototypeOf(navigator).serial;delete navigator.serial;});
 if(mode==='insecure')await page.addInitScript(()=>Object.defineProperty(window,'isSecureContext',{value:false}));
 await page.goto(url,{waitUntil:'networkidle'});
 await page.evaluate(()=>document.fonts.ready);
 return {page,errors,requests:()=>requests};
}
try{
 for(const width of widths){
  const {page,errors}=await pageAt(width);
  const manifestPath=await page.locator('esp-web-install-button').getAttribute('manifest');
  const manifestResponse=await page.request.get(new URL(manifestPath,url).href);
  let manifest;try{manifest=await manifestResponse.json();}catch{}
  check(`real manifest path and release ${width}`,manifestResponse.ok()&&manifestPath===new URL('manifest.json',url).pathname&&!!manifest?.version&&(await page.locator('#flash').innerText()).includes(`Published firmware ${manifest.version}`),{path:manifestPath,status:manifestResponse.status()});
  const geometry=await page.evaluate(()=>{
   const over=[];
   const walker=document.createTreeWalker(document.body,NodeFilter.SHOW_TEXT);
   const range=document.createRange();
   for(let n=walker.nextNode();n;n=walker.nextNode()){
    if(!n.textContent.trim()||n.parentElement.closest('script,style,dialog:not([open]),details:not([open])>*:not(summary)'))continue;
    range.selectNodeContents(n);
    if([...range.getClientRects()].some(r=>r.width>0&&(r.right>innerWidth+1||r.left< -1)))over.push(n.textContent.trim().slice(0,60));
   }
   const h=document.querySelector('h1'),r=h.getBoundingClientRect(),s=getComputedStyle(h);
   const linesOf=e=>{const tops=[],font=parseFloat(getComputedStyle(e).fontSize),w=document.createTreeWalker(e,NodeFilter.SHOW_TEXT),r=document.createRange();for(let n=w.nextNode();n;n=w.nextNode()){if(!n.textContent.trim())continue;r.selectNodeContents(n);for(const rect of r.getClientRects()){if(rect.width<1||rect.height<font*.35)continue;const mid=rect.top+rect.height/2;if(!tops.some(t=>Math.abs(t-mid)<font*.55))tops.push(mid);}}return tops.length;};
   const longHeadings=[...document.querySelectorAll('h1,h2,h3')].filter(e=>!e.closest('details:not([open]),dialog:not([open])')).map(e=>({text:e.innerText,lines:linesOf(e)})).filter(e=>e.lines>2);
   return {viewport:innerWidth,scroll:document.documentElement.scrollWidth,overflowTexts:over,titleLines:Math.round(r.height/parseFloat(s.lineHeight)),longHeadings};
  });
  check(`${width} no horizontal overflow`,geometry.scroll<=width&&geometry.overflowTexts.length===0,geometry);
  check(`${width} title fits two lines`,geometry.titleLines===2);
  check(`${width} section headings fit`,geometry.longHeadings.length===0,{headings:geometry.longHeadings});
  check(`${width} no JavaScript errors`,errors.length===0,{errors});
  await page.screenshot({path:join(out,`fold-${width}.png`)});
  if(width===390||width===1440){
   const unloaded=await page.evaluate(async()=>{const images=[...document.images];await Promise.all(images.map(async image=>{image.loading='eager';try{await image.decode();}catch{}}));return images.filter(image=>!image.naturalWidth).map(image=>image.alt);});
   check(`all page pictures decode ${width}`,unloaded.length===0,{unloaded});
   await page.screenshot({path:join(out,`full-${width}.png`),fullPage:true});
  }
  if(width===390){
   const menu=page.locator('#menu-toggle');await menu.focus();await page.keyboard.press('Enter');
   check('keyboard opens mobile menu',await menu.getAttribute('aria-expanded')==='true');
   await page.keyboard.press('ArrowDown');check('menu first link reachable with ArrowDown',await page.locator('#site-nav a').first().evaluate(e=>document.activeElement===e));
   await page.keyboard.press('Escape');check('Escape closes menu and restores focus',await menu.getAttribute('aria-expanded')==='false'&&await menu.evaluate(e=>document.activeElement===e));
   const themes=await page.locator('.theme-pill').evaluateAll(es=>es.map(e=>e.dataset.themeId));
   const viewIds=await page.locator('#view-select option').evaluateAll(es=>es.map(e=>e.value));
   check('seven themes and nine views',themes.length===7&&viewIds.length===9);
   for(const theme of themes){
    await page.locator(`.theme-pill[data-theme-id="${theme}"]`).click();
    for(const view of viewIds){
     await page.locator('#view-select').selectOption(view);
     const state=await page.evaluate(async()=>{
      const image=document.querySelector('.preview-image.active');await image.decode();
      return {theme:document.documentElement.dataset.theme,active:document.querySelectorAll('.preview-image.active').length,pressed:document.querySelectorAll('.theme-pill[aria-pressed=true]').length,view:image.dataset.view,w:image.naturalWidth,h:image.naturalHeight};
     });
     check(`gallery ${theme}/${view}`,state.theme===theme&&state.active===1&&state.pressed===1&&state.view===view&&state.w===640&&state.h===172);
    }
    await page.locator('#view-select').selectOption('stats');await page.evaluate(()=>scrollTo(0,0));
    await page.screenshot({path:join(out,`theme-${theme}.png`)});
   }
   await page.locator('#preview-enlarge').click();
   check('dialog opens with Close focus',await page.locator('#preview-dialog').evaluate(e=>e.open)&&await page.locator('#dialog-close').evaluate(e=>document.activeElement===e));
   await page.locator('#dialog-next').click();check('dialog advances view',await page.locator('#dialog-title').innerText()==='Bambu Lab');
   await page.keyboard.press('Escape');await page.waitForTimeout(50);
   check('dialog closes and restores focus',await page.locator('#preview-enlarge').evaluate(e=>document.activeElement===e)&&await page.evaluate(()=>!document.body.classList.contains('dialog-open')));
   await page.emulateMedia({reducedMotion:'reduce'});
   const motion=await page.evaluate(()=>({smooth:getComputedStyle(document.documentElement).scrollBehavior,active:document.getAnimations().filter(a=>a.playState==='running').length}));
   check('reduced motion disables nonessential motion',motion.smooth==='auto'&&motion.active===0,motion);
  }
  await page.close();
 }
 for(const mode of engine==='webkit'?['unsupported','insecure']:['unsupported','insecure','failure','recover']){
  const {page,requests}=await pageAt(390,mode);
  const status=page.locator('#js-flash-status');
  await page.waitForTimeout(50);
  if(mode==='unsupported')check('unsupported browser has one clear fallback',(await status.innerText()).includes('cannot install firmware')&&!(await page.locator('#js-flash-activate').isVisible())&&requests()===0);
  if(mode==='insecure')check('insecure context gives HTTPS fallback',(await status.innerText()).includes('HTTPS')&&!(await page.locator('#js-flash-activate').isVisible())&&requests()===0);
  if(mode==='failure')check('loader error offers retry',(await status.innerText()).includes('could not load')&&await page.locator('#js-flash-retry').isVisible());
  if(mode==='recover'){
   await page.locator('#js-flash-retry').click();await page.waitForFunction(()=>!document.getElementById('js-flash-activate').disabled);
   check('retry refetches a failed module and recovers',requests()===2&&(await status.innerText()).includes('Installer ready')&&!(await page.locator('#js-flash-retry').isVisible()));
  }
  await page.locator('#flash').scrollIntoViewIfNeeded();await page.screenshot({path:join(out,`installer-${mode}.png`)});await page.close();
 }
}finally{
 await browser.close();writeFileSync(join(out,'report.json'),JSON.stringify(report,null,2));
}
const failed=report.checks.filter(c=>!c.pass);
console.log(JSON.stringify({checks:report.checks.length,passed:report.checks.length-failed.length,failed},null,2));
process.exitCode=failed.length?1:0;
