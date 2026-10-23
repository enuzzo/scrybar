// Metadata fixtures model persistence states; this never writes a device.
import {createRequire} from 'node:module';
import {readFileSync,mkdirSync,writeFileSync} from 'node:fs';
import {resolve,join} from 'node:path';
const require=createRequire(import.meta.url);
const {chromium,webkit}=require(process.env.SCRYBAR_PLAYWRIGHT_MODULE||'playwright');
const args=process.argv.slice(2),option=(name,fallback)=>{const i=args.indexOf(name);return i<0?fallback:args[i+1];};
const url=option('--url','http://127.0.0.1:5180/'),engine=option('--engine','chromium');
if(!['127.0.0.1','localhost','[::1]'].includes(new URL(url).hostname))throw new Error('Only a loopback fixture is allowed.');
const out=resolve(option('--out','/tmp/scrybar-brightness-state-qa'));mkdirSync(out,{recursive:true});
const source=readFileSync(new URL('../../scrybar.ino',import.meta.url),'utf8');
const enhancement=source.match(/static const char kWebJsControl\[\] PROGMEM = R"rawliteral\(([\s\S]*?)\)rawliteral";/)[1].trim();
const browser=engine==='webkit'?await webkit.launch():await chromium.launch({channel:'chrome'});
const report={engine,at:new Date().toISOString(),notes:'Current production JS + captured/redacted markup. RAM/stored metadata are explicit unit fixtures, not a claim that NVS writes succeeded on hardware.',checks:[]};
const check=(name,pass)=>report.checks.push({name,pass});
try{
 for(const item of [
  {name:'reopened unsaved preview',ram:[81,60],saved:[100,60],dirty:true,undo:true},
  {name:'acknowledged stored levels',ram:[81,60],saved:[81,60],dirty:false},
  {name:'failed persistence retains old metadata',ram:[81,55],saved:[100,60],dirty:true},
  {name:'partial persistence remains dirty for battery',ram:[81,55],saved:[81,60],dirty:true}
 ]){
  const page=await browser.newPage({viewport:{width:390,height:844}});
  await page.route(url,async route=>{
   const response=await route.fetch();let html=await response.text();
   // Replace only the enhancement literal, allowing this probe before a new firmware flash.
   html=html.replace(/<script>\s*\(function\(\)\{\s*'use strict';[\s\S]*?<\/script>/,enhancement);
   ['bl_usb','bl_batt'].forEach((id,i)=>{html=html.replace(new RegExp(`<input[^>]*id=['"]${id}['"][^>]*>`),tag=>tag.replace(/\bvalue=['"][^'"]*['"]/,`value='${item.ram[i]}'`).replace(/\sdata-saved-value=['"][^'"]*['"]/,'').replace(/>$/,` data-saved-value='${item.saved[i]}'>`));});
   await route.fulfill({response,body:html});
  });
  await page.goto(url,{waitUntil:'networkidle'});
  const save=page.locator('.vm-actions-sticky button[type=submit]:not([formaction])');
  check(`${item.name}: Save state`,await save.isEnabled()===item.dirty);
  check(`${item.name}: truthful pending text`,(await page.locator('#cp-save-state').textContent())===(item.dirty?'Unsaved changes':'All changes saved'));
  if(item.undo){
   await page.locator('#bl_usb').evaluate((el,value)=>{el.value=String(value);el.dispatchEvent(new Event('input',{bubbles:true}));},item.saved[0]);
   await page.waitForTimeout(150);check('undo to stored level returns clean',await save.isDisabled()&&(await page.locator('#cp-save-state').textContent())==='All changes saved');
  }
  await page.screenshot({path:join(out,item.name.replaceAll(' ','-')+'.png')});await page.close();
 }
}catch(error){report.checks.push({name:'run completed',pass:false,error:error.message});}
finally{await browser.close();report.passed=report.checks.filter(c=>c.pass).length;report.failed=report.checks.filter(c=>!c.pass).length;writeFileSync(join(out,'report.json'),JSON.stringify(report,null,2));console.log(JSON.stringify(report,null,2));if(report.failed)process.exitCode=1;}
