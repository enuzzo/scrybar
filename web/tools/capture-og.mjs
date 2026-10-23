// Capture a new social card from the actual product DOM and anonymous native Stats render.
// Run against a local production preview; this does not edit an existing bitmap.
import {createRequire} from 'node:module';
import {resolve} from 'node:path';
const require=createRequire(import.meta.url);
const {chromium}=require(process.env.SCRYBAR_PLAYWRIGHT_MODULE||'playwright');
const url=process.argv[2]||'http://127.0.0.1:5181/';
if(!['127.0.0.1','localhost','[::1]'].includes(new URL(url).hostname))throw new Error('Use the local production preview.');
const destination=resolve(process.argv[3]||'public/og-image.jpg');
const browser=await chromium.launch({channel:'chrome'});
try{
 const page=await browser.newPage({viewport:{width:1200,height:630},deviceScaleFactor:1,reducedMotion:'reduce'});
 await page.route('https://unpkg.com/**',route=>route.abort());
 await page.goto(url,{waitUntil:'networkidle'});await page.evaluate(()=>document.fonts.ready);
 await page.addStyleTag({content:`
  .site-header,.preview-controls,.enlarge,.preview-caption>div,body>astro-dev-toolbar {display:none!important}
  body{overflow:hidden}.hero{padding:32px 0}.container{width:calc(100% - 80px);max-width:none}
  .hero-copy{grid-template-columns:1.4fr 1fr;gap:40px;margin-bottom:28px;align-items:center}
  .hero h1{font-size:62px}.hero-intro p{font-size:18px;line-height:1.5;margin:0}.hero-intro .button{display:none}
  .preview-stage{padding:20px;background:var(--bg-surface)}.display-frame{max-width:none}
  .preview-caption{display:block;margin-top:12px}.preview-note{font-size:14px;text-align:right;max-width:none}
 `});
 await page.screenshot({path:destination,type:'jpeg',quality:92});
 console.log(`Captured ${destination} at 1200×630 from the built page; native Stats sample data.`);
}finally{await browser.close();}
