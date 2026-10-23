// Run only against an explicit loopback transport fixture; never sends settings to a device.
import {createRequire} from 'node:module';
import {mkdirSync,writeFileSync} from 'node:fs';
import {resolve,join} from 'node:path';
const require=createRequire(import.meta.url);
const {chromium,webkit}=require(process.env.SCRYBAR_PLAYWRIGHT_MODULE||'playwright');
const args=process.argv.slice(2);
const option=(key,fallback)=>{const i=args.indexOf(key);return i<0?fallback:args[i+1];};
const url=option('--url','http://127.0.0.1:5180/');
if(!['127.0.0.1','localhost','[::1]'].includes(new URL(url).hostname))throw new Error('Use a loopback fixture, never the ScryBar device.');
const out=resolve(option('--out','/tmp/scrybar-control-qa'));
const engine=option('--engine','chromium');
const widths=option('--widths','320,375,390,430,768,1440').split(',').map(Number);
mkdirSync(out,{recursive:true});
const browser=engine==='webkit'?await webkit.launch():await chromium.launch({channel:'chrome'});
const report={url,engine,at:new Date().toISOString(),notes:'Local fixture. Chromium/WebKit responsive emulation, not a physical phone. No USB, actual device writes, Wi-Fi provisioning or NVS persistence.',checks:[]};
const check=(name,pass,detail={})=>report.checks.push({name,pass,...detail});
async function open(width,options={}){
 const page=await browser.newPage({viewport:{width,height:width<600?844:900},reducedMotion:options.reduced?'reduce':'no-preference',javaScriptEnabled:options.js!==false});
 page.setDefaultTimeout(10000);
 const errors=[];page.on('pageerror',error=>errors.push(error.message));
 if(options.fontBlocked)await page.route(/fonts\.(googleapis|gstatic)\.com/,route=>route.abort());
 await page.route('https://geocoding-api.open-meteo.com/**',route=>route.fulfill({status:200,contentType:'application/json',body:JSON.stringify({results:[{name:'Example City',admin1:'Example Region',country:'Example Country',latitude:-33.8688,longitude:151.2093}]})}));
 await page.route('https://api.transitous.org/**',route=>route.fulfill({status:200,contentType:'application/json',body:JSON.stringify([{type:'STOP',id:'example:stop:24',name:'Example Station',country:'XX',modes:['RAIL']}])}));
 await page.goto(url,{waitUntil:'networkidle'});if(options.js!==false)await page.evaluate(()=>document.fonts.ready);
 return {page,errors};
}
const saveSelector='.vm-actions-sticky button[type=submit]:not([formaction])';
const geometry=page=>page.evaluate(()=>{
 const bad=[];
 for(const el of document.querySelectorAll('input:not([type=hidden]),select,button,.vm-card,.cp-setting-body')){
  const r=el.getBoundingClientRect();if(r.width&&r.height&&(r.left<-.5||r.right>innerWidth+.5))bad.push({tag:el.tagName,id:el.id,left:r.left,right:r.right});
 }
 return {width:innerWidth,scroll:document.documentElement.scrollWidth,bad};
});
try{
 for(const width of widths){
  const {page,errors}=await open(width);
  check(`clean save disabled ${width}`,await page.locator(saveSelector).isDisabled());
  const themes=await page.locator('[name=ui_theme] option').evaluateAll(options=>options.map(o=>o.value));
  check(`seven actual themes ${width}`,themes.length===7);
  for(const theme of themes){
   await page.getByRole('tab',{name:'Display',exact:true}).click();
   await page.locator('[name=ui_theme]').selectOption(theme);
   for(const section of ['Display','Wi-Fi','Content']){
    await page.getByRole('tab',{name:section,exact:true}).click();const g=await geometry(page);
    check(`fits ${width} ${theme} ${section}`,g.scroll<=width+1&&g.bad.length===0,g);
   }
   if(width===390){await page.getByRole('tab',{name:'Display',exact:true}).click();await page.screenshot({path:join(out,`theme-${theme}.png`)});}
  }
  await page.getByRole('tab',{name:'Content',exact:true}).click();
  const last=page.locator('.rss-actions button').last();await last.scrollIntoViewIfNeeded();
  const clear=await last.evaluate(el=>{const r=el.getBoundingClientRect(),bar=document.querySelector('.vm-actions-sticky').getBoundingClientRect();return r.bottom<=bar.top&&r.top>=0;});
  check(`last RSS action clears fixed save bar ${width}`,clear);
  await page.locator('.cp-section-button').last().scrollIntoViewIfNeeded();await page.locator('.cp-section-button').last().click();
  const details=page.locator('#cp-device-body');await details.scrollIntoViewIfNeeded();
  check(`device details accessible ${width}`,await details.isVisible());
  await page.getByRole('tab',{name:'Content',exact:true}).click();await page.screenshot({path:join(out,`content-${width}.png`),fullPage:true});
  await page.getByRole('tab',{name:'Display',exact:true}).click();await page.evaluate(()=>scrollTo(0,0));await page.screenshot({path:join(out,`fold-${width}.png`)});
  check(`no uncaught JS ${width}`,errors.length===0,{errors});await page.close();
 }
 // Interaction checks use one fresh form, independent of the theme/geometry sweep.
 const {page,errors}=await open(390);let brightnessBody='';
 await page.route('**/api/brightness',route=>{brightnessBody=route.request().postData()||'';return route.fulfill({status:200,contentType:'application/json',body:'{"ok":true,"fixture":true}'});});
 await page.locator('#bl_usb').evaluate(el=>{el.value='55';el.dispatchEvent(new Event('input',{bubbles:true}));});
 await page.waitForTimeout(250);check('brightness uses live RAM endpoint',new URLSearchParams(brightnessBody).get('usb')==='55');
 check('range changes enable Save',await page.locator(saveSelector).isEnabled());
 const brightnessStatus=page.locator('#bl_status');if(await brightnessStatus.count())check('brightness acknowledgement announced',(await brightnessStatus.textContent()).includes('preview applied'));
 const initialOrder=await page.locator('#view_list [data-view]:not(.vm-view--internal)').evaluateAll(rows=>rows.map(r=>r.dataset.view));
 await page.locator('#view_reorder_btn').click();await page.locator('#view_list .vm-view:not(.vm-view--internal)').nth(1).locator('.vm-move').first().click();
 const changedOrder=await page.locator('#view_list [data-view]:not(.vm-view--internal)').evaluateAll(rows=>rows.map(r=>r.dataset.view));
 check('view ordering changes without saving',changedOrder[0]===initialOrder[1]&&changedOrder[1]===initialOrder[0]);
 const toggle=page.locator('#view_info_cb');await toggle.setChecked(!await toggle.isChecked());
 await page.getByRole('tab',{name:'Content',exact:true}).click();
 await page.locator('#weather_lat').fill('-33.8688');await page.locator('#weather_lon').fill('-151.2093');
 check('manual negative coordinates accepted',await page.locator('#weather_lat').inputValue()==='-33.8688'&&await page.locator('#weather_lon').inputValue()==='-151.2093');
 check('coordinates allow minus keyboard',await page.locator('#weather_lat').getAttribute('inputmode')!=='numeric'&&await page.locator('#weather_lon').getAttribute('inputmode')!=='numeric');
 await page.locator('#geo_query').fill('Example');await page.waitForFunction(()=>document.getElementById('weather_city').value==='Example City');
 check('weather search applies selected fixture coordinates',await page.locator('#weather_lat').inputValue()==='-33.8688');
 await page.locator('#transit_from_q').fill('Example');await page.waitForFunction(()=>document.getElementById('transit_from_id').value==='example:stop:24');
 check('departure search retains selected stop ID',await page.locator('#transit_from_val').inputValue()==='Example Station');
 // Preserve even an invalid, unadded feed draft on refresh.
 await page.locator('#rss_url').fill('invalid pending draft');
 const snapshot=()=>page.evaluate(()=>({url:location.href,fields:Array.from(document.querySelectorAll('input:not([type=hidden]),select')).map(el=>[el.id||el.name,el.value,el.checked]),rows:Array.from(document.querySelectorAll('#view_list [data-view]')).map(el=>el.dataset.view)}));
 const before=await snapshot();let reloadBody='not requested';
 await page.route('**/reload',route=>{reloadBody=route.request().postData()||'';return route.fulfill({status:200,contentType:'text/html',body:'Fixture acknowledgement: queued'});});
 await page.getByRole('button',{name:'Reload content',exact:true}).click();await page.waitForFunction(()=>document.getElementById('cp-action-status').dataset.state==='success');
 check('reload sends no unsaved config fields',reloadBody==='');check('reload preserves pending form and URL',JSON.stringify(before)===JSON.stringify(await snapshot()));
 check('reload announces queued request and pending edits',(await page.locator('#cp-action-status').textContent()).includes('requested')&&(await page.locator('#cp-action-status').textContent()).includes('pending'));
 await page.unroute('**/reload');await page.route('**/reload',route=>route.fulfill({status:503,body:'Fixture failure'}));
 await page.getByRole('button',{name:'Reload content',exact:true}).click();await page.waitForFunction(()=>document.getElementById('cp-action-status').dataset.state==='error');
 check('reload failure keeps pending fields',JSON.stringify(before)===JSON.stringify(await snapshot()));
 check('reload failure does not claim completion',(await page.locator('#cp-action-status').textContent()).includes('could not be confirmed'));
 await page.locator('#rss_reset').click();
 const originalFeeds=await page.locator('.rss-row').count();
 for(let i=originalFeeds;i<5;i++){
  await page.locator('#rss_name').fill('Fixture feed '+(i+1));await page.locator('#rss_url').fill(`https://example.invalid/feed-${i+1}.xml`);await page.locator('#rss_max').fill(String(i%8+1));await page.locator('#rss_add').click();
 }
 check('RSS supports five feed rows',await page.locator('.rss-row').count()===5);
 check('RSS count remains visible',(await page.locator('#rss_count_pill').textContent()).includes('5/5'));
 await page.locator('#rss_url').fill('https://example.invalid/feed-overflow.xml');await page.locator('#rss_add').click();
 check('RSS sixth feed rejected',await page.locator('.rss-row').count()===5&&(await page.locator('#rss_status').textContent()).includes('5'));
 await page.locator('#rss_reset').click();await page.locator('.rss-row').last().getByRole('button',{name:/^Edit /}).click();await page.locator('#rss_name').fill('Updated fixture');await page.locator('#rss_add').click();
 check('RSS edit updates existing row',await page.locator('.rss-row').count()===5&&(await page.locator('.rss-row').last().textContent()).includes('Updated fixture'));
 await page.locator('.rss-row').last().getByRole('button',{name:/^Remove /}).click();check('RSS remove updates list and count',await page.locator('.rss-row').count()===4&&(await page.locator('#rss_count_pill').textContent()).includes('4/5'));
 await page.locator('#rss_url').fill('https://example.invalid/not-added.xml');await page.locator(saveSelector).click();
 check('Save guards an unadded feed draft',await page.locator('#rss_add').evaluate(el=>el===document.activeElement)&&(await page.locator('#rss_status').textContent()).includes('before saving'));
 await page.locator('#rss_reset').click();
 // Capture the actual native-submit contract; fulfill locally, never persist.
 let savedFields=[];
 await page.route('**/config',route=>{savedFields=[...new URLSearchParams(route.request().postData()||'').keys()];return route.fulfill({status:200,contentType:'text/html',body:'Fixture save received'});});
 await page.locator(saveSelector).click();await page.waitForURL('**/config');
 check('Save serializes view order and all five RSS slots',savedFields.includes('view_order')&&savedFields.includes('rss_feed_url_5')&&savedFields.includes('rss_feed_items_5')&&savedFields.includes('weather_lat')&&savedFields.includes('ui_theme'),{fieldNames:savedFields});
 check('interaction JS errors absent',errors.length===0,{errors});await page.close();
 // Keyboard and fallback states have their own clean pages.
 const keys=await open(390);await keys.page.getByRole('tab',{name:'Display',exact:true}).focus();await keys.page.keyboard.press('ArrowRight');
 check('tabs support arrow keys and selected focus',await keys.page.getByRole('tab',{name:'Wi-Fi',exact:true}).evaluate(el=>el===document.activeElement&&el.getAttribute('aria-selected')==='true'));
 const disclosure=keys.page.getByRole('button',{name:'Wi-Fi connections',exact:true});await disclosure.focus();await keys.page.keyboard.press('Enter');check('disclosure supports keyboard and expanded state',await disclosure.getAttribute('aria-expanded')==='false');await keys.page.close();
 const fallback=await open(320,{fontBlocked:true,reduced:true});await fallback.page.getByRole('tab',{name:'Content',exact:true}).click();const fg=await geometry(fallback.page);
 check('font-blocked 320 layout fits',fg.scroll<=320&&fg.bad.length===0,fg);check('reduced motion has no running animations',await fallback.page.evaluate(()=>document.getAnimations().filter(a=>a.playState==='running').length===0));await fallback.page.screenshot({path:join(out,'font-blocked-reduced-320.png'),fullPage:true});await fallback.page.close();
 const nojs=await open(320,{js:false});check('no-JS Save remains enabled',await nojs.page.locator(saveSelector).isEnabled());check('no-JS fields remain available',await nojs.page.locator('#weather_lat').isVisible()&&await nojs.page.locator('[name=ui_theme]').isVisible());await nojs.page.close();
}catch(error){
 check('QA run completed',false,{error:error.message});
}finally{
 await browser.close();report.passed=report.checks.filter(c=>c.pass).length;report.failed=report.checks.filter(c=>!c.pass).length;writeFileSync(join(out,'report.json'),JSON.stringify(report,null,2));console.log(JSON.stringify({engine,passed:report.passed,failed:report.failed,failures:report.checks.filter(c=>!c.pass)},null,2));if(report.failed)process.exitCode=1;
}
