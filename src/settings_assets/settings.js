(function(){
(function(){const t=document.querySelector('.vm-toast-fixed');if(t){setTimeout(function(){t.style.transition='opacity .6s';t.style.opacity='0';setTimeout(function(){t.style.display='none';},650);},4200);}})();
const q=document.getElementById('geo_query');const dl=document.getElementById('geo_hits');const st=document.getElementById('geo_status');const city=document.getElementById('weather_city');const lat=document.getElementById('weather_lat');const lon=document.getElementById('weather_lon');
if(!q||!dl||!city||!lat||!lon)return;
let timer,revision=0;
const cities=new Map();
function setStatus(message){if(st)st.textContent=message;}
function clearHits(){dl.replaceChildren();cities.clear();}
function applyPick(){
  const result=cities.get(q.value);
  if(!result)return false;
  clearTimeout(timer);revision++;
  city.value=result.name;lat.value=Number(result.latitude).toFixed(4);lon.value=Number(result.longitude).toFixed(4);
  clearHits();setStatus('Coordinates filled in automatically.');return true;
}
q.addEventListener('change',applyPick);
q.addEventListener('input',function(){
  clearTimeout(timer);const current=++revision;
  if(applyPick())return;
  clearHits();setStatus('');
  const term=q.value.trim();if(term.length<2)return;
  timer=setTimeout(async function(){
    try{
      setStatus('Searching...');
      const url='https://geocoding-api.open-meteo.com/v1/search?count=6&language=en&format=json&name='+encodeURIComponent(term);
      const response=await fetch(url,{cache:'no-store'});if(!response.ok)throw new Error();
      const data=await response.json();if(current!==revision)return;
      for(const result of data.results||[]){
        const place=[result.name,result.admin1,result.country].filter(Boolean).join(', ');
        const coordinates=Number(result.latitude).toFixed(4)+', '+Number(result.longitude).toFixed(4);
        const label=cities.has(place)?place+' ('+coordinates+')':place;
        const option=document.createElement('option');option.value=label;option.label=coordinates;
        dl.appendChild(option);cities.set(label,result);
      }
      setStatus(cities.size?'Select a result to fill city and coordinates.':'No results found.');
    }catch(error){if(current===revision)setStatus('Search unavailable, try again later.');}
  },280);
});
const wifiScanBtn=document.getElementById('wifi_scan_btn');const wifiScanResults=document.getElementById('wifi_scan_results');const wifiScanStatus=document.getElementById('wifi_scan_status');const wifiNewSsid=document.getElementById('wifi_new_ssid');const wifiNewPassword=document.getElementById('wifi_new_password');const wifiPwdToggle=document.getElementById('wifi_pwd_toggle');
function setWifiStatus(msg){if(wifiScanStatus)wifiScanStatus.textContent=msg||'';}
async function scanWifiNow(){
  if(!wifiScanResults||wifiScanBtn.disabled)return;
  wifiScanBtn.disabled=true;
  const controller=window.AbortController?new AbortController():null;
  const timeout=controller?window.setTimeout(()=>controller.abort(),9000):0;
  try{
    setWifiStatus('Scanning 2.4 GHz networks...');
    wifiScanResults.replaceChildren(new Option('Scanning...',''));
    const response=await fetch('/api/wifi/scan',{cache:'no-store',signal:controller?controller.signal:undefined});
    if(!response.ok)throw new Error();
    const data=await response.json(),networks=data.networks||[];
    if(data.message==='scan_failed')throw new Error('scan failed');
    if(!networks.length){
      const timedOut=data.message==='scan_timeout';
      wifiScanResults.replaceChildren(new Option(timedOut?'Scan timed out':'No network found',''));
      setWifiStatus(timedOut?'Scan timed out, retry in a few seconds.':'No 2.4 GHz networks found.');
      return;
    }
    wifiScanResults.replaceChildren(new Option('Choose a network',''));
    for(const network of networks){
      const label=(network.ssid||'(hidden)')+'  '+(network.secure?'SEC':'OPEN')+'  ch'+(network.channel||'?')+'  '+(network.rssi||0)+'dBm';
      const option=new Option(label,network.ssid||'');
      option.dataset.secure=network.secure?'1':'0';
      option.disabled=!network.ssid;
      wifiScanResults.add(option);
    }
    setWifiStatus('Scan complete. Pick an SSID and press Save changes.');
  }catch(error){
    wifiScanResults.replaceChildren(new Option('Scan failed',''));
    setWifiStatus(error.name==='AbortError'?'Scan timeout. Retry.':'Scan unavailable right now.');
  }finally{
    window.clearTimeout(timeout);
    wifiScanBtn.disabled=false;
  }
}
function syncWifiPwdToggle(){if(!wifiPwdToggle||!wifiNewPassword)return;const visible=wifiNewPassword.type==='text';wifiPwdToggle.textContent=visible?'Hide':'Show';wifiPwdToggle.title=visible?'Hide password':'Show password';wifiPwdToggle.setAttribute('aria-label',wifiPwdToggle.title);}if(wifiPwdToggle&&wifiNewPassword){wifiPwdToggle.addEventListener('click',function(){wifiNewPassword.type=(wifiNewPassword.type==='password')?'text':'password';syncWifiPwdToggle();});syncWifiPwdToggle();}if(wifiScanBtn)wifiScanBtn.addEventListener('click',function(){scanWifiNow();});if(wifiScanResults)wifiScanResults.addEventListener('change',function(){const v=wifiScanResults.value||'';if(wifiNewSsid&&v)wifiNewSsid.value=v;const sel=wifiScanResults.options[wifiScanResults.selectedIndex];if(wifiNewPassword&&sel&&sel.dataset.secure==='0'){wifiNewPassword.value='';wifiNewPassword.placeholder='Open network (no password)';}else if(wifiNewPassword){wifiNewPassword.placeholder='Password (WPA/WPA2)';}});
const form=document.getElementById('cfg_form');const rssName=document.getElementById('rss_name');const rssUrl=document.getElementById('rss_url');const rssMax=document.getElementById('rss_max');const rssAdd=document.getElementById('rss_add');const rssReset=document.getElementById('rss_reset');const rssList=document.getElementById('rss_list');const rssEmpty=document.getElementById('rss_empty');const rssStatus=document.getElementById('rss_status');const rssHidden=document.getElementById('rss_hidden_inputs');const rssPill=document.getElementById('rss_count_pill');const viewHidden=document.getElementById('view_hidden_inputs');const viewList=document.getElementById('view_list');const viewReorderBtn=document.getElementById('view_reorder_btn');const viewOrderStatus=document.getElementById('view_order_status');
const maxSlots=5;const minPosts=1;const maxPosts=8;let editIndex=-1;
let feeds=scrybarInitialFeeds.filter(f=>f&&f.url&&/^https?:\/\//i.test(f.url));
function clampPosts(n){n=parseInt(n,10);if(isNaN(n))n=maxPosts;if(n<minPosts)n=minPosts;if(n>maxPosts)n=maxPosts;return n;}function startsHttp(v){return /^https?:\/\//i.test((v||'').trim());}
function defName(i){return 'Feed '+(i+1);}function setRssStatus(m){if(rssStatus)rssStatus.textContent=m||'';}
function clearComposer(){editIndex=-1;rssName.value='';rssUrl.value='';rssMax.value='8';rssAdd.innerHTML="+ Add";setRssStatus('');}
function renderFeeds(){if(!rssList)return;rssList.innerHTML='';if(rssPill)rssPill.innerHTML="RSS feeds "+feeds.length+'/5';if(rssEmpty)rssEmpty.style.display=feeds.length?'none':'block';feeds.forEach(function(f,idx){const row=document.createElement('div');row.className='rss-row';const left=document.createElement('div');const t=document.createElement('p');t.className='rss-title';t.textContent='';t.appendChild(document.createTextNode(f.name||defName(idx)));const chip=document.createElement('span');chip.className='rss-chip';chip.textContent='max '+clampPosts(f.max);t.appendChild(chip);const m=document.createElement('p');m.className='rss-meta';m.textContent=f.url||'';left.appendChild(t);left.appendChild(m);const act=document.createElement('div');act.className='rss-actions';const bEdit=document.createElement('button');bEdit.type='button';bEdit.className='vm-btn vm-btn--sm vm-btn--warn';bEdit.textContent='Edit';bEdit.addEventListener('click',function(){editIndex=idx;rssName.value=f.name||'';rssUrl.value=f.url||'';rssMax.value=String(clampPosts(f.max));rssAdd.textContent='Update';setRssStatus('Editing feed '+(idx+1));});const bDel=document.createElement('button');bDel.type='button';bDel.className='vm-btn vm-btn--sm vm-btn--danger';bDel.textContent='Delete';bDel.addEventListener('click',function(){feeds.splice(idx,1);if(editIndex===idx)clearComposer();else if(editIndex>idx)editIndex-=1;renderFeeds();setRssStatus('Feed removed.');});act.appendChild(bEdit);act.appendChild(bDel);row.appendChild(left);row.appendChild(act);rssList.appendChild(row);});}
function pushOrUpdate(){const name=(rssName.value||'').trim();const url=(rssUrl.value||'').trim();const max=clampPosts(rssMax.value);if(!url){setRssStatus('Please enter a feed URL.');return;}if(!startsHttp(url)){setRssStatus('URL must start with http:// or https://');return;}const item={name:name||defName(editIndex>=0?editIndex:feeds.length),url:url,max:max};if(editIndex>=0){feeds[editIndex]=item;clearComposer();setRssStatus('Feed updated.');renderFeeds();return;}if(feeds.length>=maxSlots){setRssStatus('Maximum limit: 5 feeds.');return;}feeds.push(item);clearComposer();renderFeeds();setRssStatus('Feed added.');}
function addHidden(k,v){const i=document.createElement('input');i.type='hidden';i.name=k;i.value=v;rssHidden.appendChild(i);}function buildHiddenInputs(){if(!rssHidden)return;rssHidden.innerHTML='';for(let i=0;i<maxSlots;i+=1){const f=feeds[i]||{name:defName(i),url:'',max:maxPosts};addHidden('rss_feed_name_'+(i+1),f.name||defName(i));addHidden('rss_feed_url_'+(i+1),f.url||'');addHidden('rss_feed_items_'+(i+1),String(clampPosts(f.max)));}const f0=feeds[0]||{name:defName(0),url:'',max:maxPosts};addHidden('rss_feed_name',f0.name||defName(0));addHidden('rss_feed_url',f0.url||'');addHidden('rss_feed_items',String(clampPosts(f0.max)));}
function addViewHidden(k,v){if(!viewHidden)return;const i=document.createElement('input');i.type='hidden';i.name=k;i.value=v;viewHidden.appendChild(i);}function buildViewHiddenInputs(){if(!viewHidden)return;viewHidden.innerHTML='';[['view_info','view_info_cb'],['view_aux','view_aux_cb'],['view_wiki','view_wiki_cb'],['view_now_playing','view_now_playing_cb'],['view_doom','view_doom_cb'],['view_transit','view_transit_cb'],['view_launch','view_launch_cb'],['view_mac_stats','view_mac_stats_cb']].forEach(function(pair){const el=document.getElementById(pair[1]);addViewHidden(pair[0],(el&&el.checked)?'1':'0');});if(viewList){const order=Array.from(viewList.querySelectorAll('[data-view]')).map(function(row){return row.dataset.view;}).join(',');addViewHidden('view_order',order);}}
function updateViewMoveButtons(){if(!viewList)return;const rows=Array.from(viewList.querySelectorAll('[data-view]:not(.vm-view--internal)'));rows.forEach(function(row,idx){const buttons=row.querySelectorAll('.vm-move');if(buttons[0])buttons[0].disabled=(idx===0);if(buttons[1])buttons[1].disabled=(idx===rows.length-1);});}
function setViewReorderMode(on){if(!viewList||!viewReorderBtn)return;viewList.classList.toggle('is-reordering',on);viewReorderBtn.setAttribute('aria-pressed',on?'true':'false');viewReorderBtn.textContent=on?'Done':'Reorder';if(viewOrderStatus)viewOrderStatus.classList.toggle('is-visible',on);if(on){const card=viewList.closest('.vm-card');if(card){card.classList.remove('vm-clp');const arrow=card.querySelector('.vm-collapse-arr');if(arrow)arrow.textContent='\u25BC';}}updateViewMoveButtons();}
if(viewReorderBtn)viewReorderBtn.addEventListener('click',function(){setViewReorderMode(!viewList.classList.contains('is-reordering'));});
if(viewList)viewList.addEventListener('click',function(e){const button=e.target.closest&&e.target.closest('.vm-move');if(!button)return;const row=button.closest('[data-view]');if(!row)return;const rows=Array.from(viewList.querySelectorAll('[data-view]:not(.vm-view--internal)'));const idx=rows.indexOf(row);const dir=parseInt(button.dataset.dir||'0',10);const target=rows[idx+dir];if(!target)return;if(dir<0)viewList.insertBefore(row,target);else viewList.insertBefore(target,row);updateViewMoveButtons();const name=row.querySelector('strong');if(viewOrderStatus)viewOrderStatus.textContent=(name?name.textContent:'View')+' moved to position '+(idx+dir+1)+'. Save changes to keep this order.';});
if(viewList)viewList.addEventListener('change',function(e){if(!e.target.matches('input[type=checkbox]'))return;const row=e.target.closest('[data-view]');if(row)row.classList.toggle('vm-view--off',!e.target.checked);});
if(rssAdd)rssAdd.addEventListener('click',function(){pushOrUpdate();});if(rssReset)rssReset.addEventListener('click',function(){clearComposer();setRssStatus('Composer cleared.');});if(form)form.addEventListener('submit',function(){buildHiddenInputs();buildViewHiddenInputs();});renderFeeds();updateViewMoveButtons();
})();

(function(){
  'use strict';
  var form=document.getElementById('cfg_form');
  if(!form)return;
  var themeSelect=form.querySelector('[name="ui_theme"]');
  var dirty=false,submitting=false,queued=false;
  var sections=[
    {id:'display',label:'Display',items:[['[name="ui_theme"]','Theme'],['#bl_usb','Brightness'],['#screensaver_minutes','Display inactivity'],['#view_list','Views and swipe order'],['[name="wc_lang"]','Display language']]},
    {id:'connections',label:'Wi-Fi',items:[['[name="wifi_pref_ssid"]','Wi-Fi connections']]},
    {id:'content',label:'Content',items:[['#weather_city','Weather'],['[name="wiki_lang"]','Wikipedia'],['#transit_from_q','Departures'],['#rss_list','RSS feeds']]}
  ];
  var nav=document.createElement('nav');nav.className='cp-nav';nav.setAttribute('aria-label','Settings sections');nav.setAttribute('role','tablist');
  var first=form.querySelector('.vm-card');
  if(first)form.insertBefore(nav,first);
  var tabs=[],panels=[];
  function showSection(id,focus){
    tabs.forEach(function(tab,i){var active=sections[i].id===id;tab.setAttribute('aria-selected',String(active));tab.tabIndex=active?0:-1;panels[i].hidden=!active;});
    if(focus){var selected=tabs.find(function(t){return t.getAttribute('aria-selected')==='true';});if(selected)selected.focus();}
  }
  function wrapSetting(card,title,id,open){
    if(!card)return;
    var old=card.querySelector('h2');if(!old)return;
    var heading=document.createElement('h2');
    var button=document.createElement('button');button.type='button';button.className='cp-section-button';button.id=id+'-heading';
    var text=document.createElement('span');text.textContent=title;
    var arrow=document.createElement('span');arrow.className='vm-collapse-arr';arrow.setAttribute('aria-hidden','true');
    button.appendChild(text);var badge=old.querySelector('.vm-badge');if(badge)button.appendChild(badge);button.appendChild(arrow);heading.appendChild(button);card.replaceChild(heading,old);
    var content=document.createElement('div');content.className='cp-setting-body';content.id=id+'-body';
    Array.from(card.childNodes).forEach(function(node){if(node!==heading)content.appendChild(node);});
    card.appendChild(content);card.classList.add('cp-setting');
    button.setAttribute('aria-controls',content.id);
    function setOpen(on){content.hidden=!on;card.classList.toggle('vm-clp',!on);button.setAttribute('aria-expanded',String(on));arrow.textContent=on?'−':'+';}
    button.addEventListener('click',function(){setOpen(content.hidden);});
    setOpen(open);return {card:card,content:content,open:function(){setOpen(true);}};
  }
  var settings=[];
  sections.forEach(function(section,i){
    var tab=document.createElement('button');tab.type='button';tab.id='cp-tab-'+section.id;tab.textContent=section.label;tab.setAttribute('role','tab');tab.setAttribute('aria-controls','cp-'+section.id);
    nav.appendChild(tab);tabs.push(tab);
    var panel=document.createElement('section');panel.id='cp-'+section.id;panel.className='cp-page';panel.setAttribute('role','tabpanel');panel.setAttribute('aria-labelledby',tab.id);form.insertBefore(panel,nav.nextSibling);panels.push(panel);
    section.items.forEach(function(item,j){var field=document.querySelector(item[0]);var card=field&&field.closest('.vm-card');if(!card)return;panel.appendChild(card);var wrapped=wrapSetting(card,item[1],'cp-'+section.id+'-'+j,true);if(wrapped)settings.push(wrapped);});
    tab.addEventListener('click',function(){showSection(section.id,false);});
    tab.addEventListener('keydown',function(e){var next=i;if(e.key==='ArrowRight')next=(i+1)%tabs.length;else if(e.key==='ArrowLeft')next=(i+tabs.length-1)%tabs.length;else if(e.key==='Home')next=0;else if(e.key==='End')next=tabs.length-1;else return;e.preventDefault();showSection(sections[next].id,true);});
  });
  showSection('display',false);
  var system=document.querySelector('.vm-card--muted');
  if(system)wrapSetting(system,'Device details','cp-device',false);
  var state=document.createElement('p');state.id='cp-save-state';state.className='cp-save-state';state.setAttribute('role','status');state.setAttribute('aria-live','polite');state.textContent='All changes saved';

  var actionStatus=document.createElement('p');actionStatus.id='cp-action-status';actionStatus.className='cp-action-status';actionStatus.setAttribute('role','status');actionStatus.setAttribute('aria-live','polite');
  var actions=form.querySelector('.vm-actions-sticky');
  if(actions){
    var feedback=document.createElement('div');feedback.className='cp-feedback';
    feedback.append(state,actionStatus);actions.prepend(feedback);
    var save=actions.querySelector('button:not([formaction])');if(save)save.textContent='Save changes';
    var reload=actions.querySelector('[formaction="/reload"]');if(reload){reload.textContent='Reload content';reload.formNoValidate=true;}
  }
  function refineLabels(){
    document.querySelectorAll('.vm-view').forEach(function(row){
      var name=row.querySelector('strong')?.textContent||'view';
      row.querySelectorAll('.vm-move').forEach(function(button){button.setAttribute('aria-label','Move '+name+(button.dataset.dir==='-1'?' up':' down'));});
    });
    var add=document.getElementById('rss_add');if(add){var text=add.textContent.trim();var value=text==='Update'||text==='Update feed'?'Update feed':'Add feed';if(add.textContent!==value)add.textContent=value;}
    var clear=document.getElementById('rss_reset');if(clear)clear.textContent='Clear fields';
    document.querySelectorAll('.rss-row').forEach(function(row){
      var name=row.querySelector('.rss-title')?.firstChild?.textContent||'feed';
      row.querySelectorAll('.rss-actions button').forEach(function(button,i){button.textContent=i?'Remove':'Edit';button.setAttribute('aria-label',(i?'Remove ':'Edit ')+name.trim());});
    });
  }
  function applyTheme(id){
    var theme=scrybarWebThemes.find(function(t){return t.id===id;});if(!theme)return;
    Object.keys(theme.vars).forEach(function(key){document.documentElement.style.setProperty(key,theme.vars[key]);});
    document.documentElement.dataset.uiTheme=id;
    var preview=document.getElementById('theme_sample');
    if(preview){
      preview.alt=themeSelect.options[themeSelect.selectedIndex].text+' Home preview with example weather';
      var url=new URL(preview.src);
      if(url.searchParams.get('theme')!==id){url.searchParams.set('theme',id);preview.src=url.href;}
    }
  }
  if(themeSelect){applyTheme(themeSelect.value);themeSelect.addEventListener('change',function(){applyTheme(themeSelect.value);});}
  function fingerprint(useSavedBrightness){
    var values=Array.from(new FormData(form).entries()).filter(function(pair){return !pair[0].startsWith('rss_feed_')&&!pair[0].startsWith('view_');});
    if(useSavedBrightness)values=values.map(function(pair){var field=(pair[0]==='bl_usb'||pair[0]==='bl_batt')&&document.getElementById(pair[0]);return field&&field.dataset.savedValue!==undefined?[pair[0],field.dataset.savedValue]:pair;});
    var views=Array.from(document.querySelectorAll('#view_list .vm-view')).map(function(row){return [row.dataset.view,!!row.querySelector('input')?.checked];});
    var feeds=Array.from(document.querySelectorAll('.rss-row')).map(function(row){return [row.querySelector('.rss-title')?.textContent,row.querySelector('.rss-meta')?.textContent];});
    return JSON.stringify([values,views,feeds]);
  }
  refineLabels();
  // A fresh GET may contain a live RAM preview which has not been stored in NVS.
  var baseline=fingerprint(true);
  function hasFeedDraft(){return !!(document.getElementById('rss_name')?.value.trim()||document.getElementById('rss_url')?.value.trim());}
  function updateDirty(){queued=false;refineLabels();dirty=form.dataset.savePending==='1'||fingerprint()!==baseline;state.textContent=hasFeedDraft()?'Feed draft: add it to the list before saving':form.dataset.savePending==='1'?'Storage write failed. Save again to retry.':dirty?'Unsaved changes':'All changes saved';document.documentElement.classList.toggle('cp-dirty',dirty||hasFeedDraft());if(save)save.disabled=!dirty&&!hasFeedDraft();}
  updateDirty();
  function schedule(){if(queued)return;queued=true;queueMicrotask(updateDirty);}
  form.addEventListener('input',schedule);form.addEventListener('change',schedule);form.addEventListener('click',schedule);
  var rssList=document.getElementById('rss_list'),viewList=document.getElementById('view_list');
  if(rssList)new MutationObserver(schedule).observe(rssList,{childList:true,subtree:false});
  if(viewList)new MutationObserver(schedule).observe(viewList,{childList:true});
  var saverDelay=document.getElementById('screensaver_minutes'),offDelay=document.getElementById('screen_off_minutes');
  if(saverDelay&&offDelay){
    function validateIdleDelays(){
      var wholeMinutes=/^[0-9]+$/;
      var wholeMinutesMessage='Enter a whole number of minutes.';
      saverDelay.setCustomValidity(saverDelay.value&&!wholeMinutes.test(saverDelay.value)?wholeMinutesMessage:'');
      offDelay.setCustomValidity(offDelay.value&&!wholeMinutes.test(offDelay.value)?wholeMinutesMessage:'');
      if(saverDelay.validity.valid&&offDelay.validity.valid&&saverDelay.valueAsNumber>0&&offDelay.valueAsNumber>0&&offDelay.valueAsNumber<=saverDelay.valueAsNumber)
        offDelay.setCustomValidity('Screen off must be later than the screensaver.');
    }
    saverDelay.addEventListener('input',validateIdleDelays);
    offDelay.addEventListener('input',validateIdleDelays);
    validateIdleDelays();
  }
  // Autocomplete and scan responses fill fields programmatically after their input event.
  ['geo_status','wifi_scan_status','transit_from_st'].forEach(function(id){var status=document.getElementById(id);if(status)new MutationObserver(schedule).observe(status,{childList:true,characterData:true,subtree:true});});
  form.addEventListener('invalid',function(e){var panel=e.target.closest('.cp-page');if(panel)showSection(panel.id.replace('cp-',''),false);var setting=settings.find(function(s){return s.card.contains(e.target);});if(setting)setting.open();},true);
  form.addEventListener('submit',async function(e){
    if(e.submitter&&e.submitter.getAttribute('formaction')==='/reload'){
      e.preventDefault();
      var button=e.submitter;button.disabled=true;button.setAttribute('aria-busy','true');
      actionStatus.dataset.state='pending';actionStatus.textContent='Requesting a content refresh…';
      try{
        // Empty POST intentionally queues a refresh without applying unsaved fields.
        var response=await fetch('/reload',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'',credentials:'same-origin',cache:'no-store'});
        if(!response.ok)throw new Error('Refresh request failed');
        actionStatus.dataset.state='success';actionStatus.textContent='Content refresh requested.'+((dirty||hasFeedDraft())?' Your unsaved changes are still pending.':'');
      }catch(error){actionStatus.dataset.state='error';actionStatus.textContent='The refresh request could not be confirmed. Your choices are still here; try again.';}
      finally{button.disabled=false;button.removeAttribute('aria-busy');}
      return;
    }
    if(hasFeedDraft()){
      e.preventDefault();showSection('content',false);
      var setting=settings.find(function(s){return s.card.contains(document.getElementById('rss_add'));});if(setting)setting.open();
      var status=document.getElementById('rss_status');if(status)status.textContent='Add or update the feed in the list, or clear the fields, before saving.';
      document.getElementById('rss_add').focus();updateDirty();return;
    }
    submitting=true;state.textContent='Saving changes…';});
  window.addEventListener('beforeunload',function(e){if((dirty||hasFeedDraft())&&!submitting){e.preventDefault();e.returnValue='';}});
})();
