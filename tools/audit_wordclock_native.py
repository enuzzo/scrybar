#!/usr/bin/env python3
"""Finite actual word-clock/date extraction and HOME font-fit audit; no device IO."""
from __future__ import annotations
import argparse
import json
import pathlib
import re
import sys
import tempfile
sys.dont_write_bytecode=True
from render_display_quality_native import generated_source
from render_mac_stats_native import ROOT,block,function,run

LANGUAGES={"it":"It","en":"En","fr":"Fr","de":"De","es":"Es","pt":"Pt","la":"La",
           "eo":"Eo","l33t":"L33t","sha":"Sha","val":"Val","bellazio":"Bellazio","pir":"Pir","tlh":"Tlh"}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out-prefix',type=pathlib.Path,default=ROOT/'docs/design/2026-10-23-display-quality/wordclock')
    parser.add_argument('--build-dir',type=pathlib.Path,default=pathlib.Path(tempfile.gettempdir())/'scrybar-display-quality-native')
    args=parser.parse_args()
    source=(ROOT/'scrybar.ino').read_text();strings=(ROOT/'src/ui_strings.h').read_text()
    cpp,themes,metadata=generated_source(source,(ROOT/'config.h').read_text(),strings)
    selected={}
    def collect(name):
        if name in selected:return
        code=function(source,name);selected[name]=code
        for called in re.findall(r'\b(\w+)\s*\(',code):
            if called!=name and (called.startswith(('wordHour','wordMinute','shaExclaim','pirate','bellazio'))):collect(called)
    for suffix in LANGUAGES.values():
        collect('composeWordClockSentence'+suffix);collect('formatDate'+suffix)
    declarations=block(source,source.index('enum UiClockMode : uint8_t {'))+';\n'
    declarations+=block(source,source.index('struct ClockState {'))+';\nstatic ClockState g_clock;\n'
    declarations+=block(source,source.index('struct BellazioCloser {'))+';\n'
    for beginning in ['static const char* const kBellazioLeads','static const BellazioCloser kBellazioClosers']:
        declarations+=block(source,source.index(beginning))+';\n'
    declarations+='\n'.join(c[:c.index('{')].strip()+';' for c in selected.values())+'\n'
    declarations+='\n'.join(selected.values())
    dispatch='\n'.join('{"'+code+'",composeWordClockSentence'+suffix+',formatDate'+suffix+'},'for code,suffix in LANGUAGES.items())
    ui_update=function(source,'updateLvglUi')
    pipeline_ascii='sanitizeAsciiBuffer(sentence,'in ui_update
    capacities=re.search(r'char sentence\[(\d+)\], d1\[(\d+)\]',ui_update)
    if not capacities:raise ValueError('Production clock/date buffers changed; review the probe')
    clock_capacity,date_capacity=map(int,capacities.groups())
    harness=r'''
struct Language{const char*code;void(*clock)(const tm&,char*,size_t);void(*date)(const tm&,char*,size_t);};
static const Language languages[]={DISPATCH};
static unsigned missingGlyphs(const char*text,const lv_font_t*font){
  unsigned missing=0;uint32_t index=0;
  while(text[index]){uint32_t cp=_lv_txt_encoded_next(text,&index);if(cp=='\n'||cp=='\r')continue;
    lv_font_glyph_dsc_t d={};if(!font->get_glyph_dsc(font,&d,cp,0))++missing;}
  return missing;
}
int main(){
 lv_init();static lv_color_t pixels[640*18];static lv_disp_draw_buf_t buffer;lv_disp_draw_buf_init(&buffer,pixels,nullptr,640*18);
 static lv_disp_drv_t driver;lv_disp_drv_init(&driver);driver.hor_res=640;driver.ver_res=172;driver.draw_buf=&buffer;driver.flush_cb=nativeFlush;
 auto*display=lv_disp_drv_register(&driver);if(!display)return 2;
 lv_obj_remove_style_all(lv_scr_act());lv_obj_set_size(lv_scr_act(),640,172);lv_obj_clear_flag(lv_scr_act(),LV_OBJ_FLAG_SCROLLABLE);
 g_lvglHomeRoot=lvglCreatePageRoot(lv_scr_act(),640,172);qualityHome();
 const int hours[]={0,1,2,12,23},minutes[]={0,1,2,3,14,15,29,30,44,45,57,58,59};
 for(const auto&language:languages)for(int hour:hours)for(int minute:minutes){
   tm t={};t.tm_year=126;t.tm_mon=9;t.tm_mday=23;t.tm_wday=5;t.tm_yday=295;t.tm_hour=hour;t.tm_min=minute;
   struct Guard{uint32_t before=0xBA37C951;char text[CLOCK_CAPACITY];uint32_t after=0xF14C62A9;}guard;
   memset(guard.text,0x7f,sizeof guard.text);char raw[1024]={},date[1024]={},productionDate[DATE_CAPACITY]={};
   language.clock(t,raw,sizeof raw);language.clock(t,guard.text,sizeof guard.text);
   if(!memchr(guard.text,0,sizeof guard.text)||guard.before!=0xBA37C951||guard.after!=0xF14C62A9)return 3;
   language.date(t,date,sizeof date);language.date(t,productionDate,sizeof productionDate);
   char sentence[CLOCK_CAPACITY];snprintf(sentence,sizeof sentence,"%s",guard.text);
   if(PIPELINE_ASCII)sanitizeAsciiBuffer(sentence,sizeof sentence);
   if(sentence[0]>='a'&&sentence[0]<='z')sentence[0]=(char)toupper((unsigned char)sentence[0]);
   lvglApplyClockSentenceAutoFit(sentence);lv_obj_update_layout(lv_scr_act());
   lv_area_t a;lv_obj_get_coords(g_clockUi.l1,&a);const lv_font_t*font=lv_obj_get_style_text_font(g_clockUi.l1,0);
   std::printf("CLOCK\t%s\t%02d:%02d\t%d\t%d\t%u\t%u\t%d\t%d\t%d\t%d\t%s\t%s\t%s\t%s\n",
    language.code,hour,minute,strcmp(raw,guard.text)!=0,strcmp(date,productionDate)!=0,
    missingGlyphs(raw,font),font->line_height,a.x1,a.y1,a.x2,a.y2,raw,sentence,date,productionDate);
 }
 for(uint32_t cp:{uint32_t(0x00C8),uint32_t(0x00EC),uint32_t(0x016D),uint32_t(0x015D)}){
  for(const lv_font_t*font:{&scry_font_dosis_32,&scry_font_dosis_16,&scry_font_dosis_semibold_20}){
   lv_font_glyph_dsc_t d={};std::printf("GLYPH\t%04x\t%u\t%d\n",cp,font->line_height,font->get_glyph_dsc(font,&d,cp,0));}}
 lv_disp_remove(display);
}
'''.replace('DISPATCH',dispatch).replace('PIPELINE_ASCII','true'if pipeline_ascii else'false').replace('CLOCK_CAPACITY',str(clock_capacity)).replace('DATE_CAPACITY',str(date_capacity))
    build=args.build_dir.resolve()
    if 'Dropbox'in build.parts:raise ValueError('Native builds must stay outside Dropbox')
    if not (build/'lvgl/lib/liblvgl.a').exists():raise ValueError('Run render_display_quality_native.py first to prepare LVGL/fonts')
    (build/'quality_ui_strings.h').write_text(strings)
    fixture=(ROOT/'tests/fixtures/display_quality_native.cpp').read_text()
    (build/'wordclock.cpp').write_text(cpp+'\n#define main quality_pages_main\n'+fixture+'\n#undef main\n'+declarations+'\n'+harness)
    lvgl=pathlib.Path.home()/'Documents/Arduino/libraries/lvgl'
    fonts=sorted(set(re.findall(r'LV_FONT_DECLARE\((scry_font_dosis_\w+)\)',cpp)))
    run(['c++','-std=c++17','-O2','-Wno-c99-designator','-I',str(ROOT),'-I',str(build),'-I',str(lvgl),str(build/'wordclock.cpp'),
         *[str(build/(font+'.o'))for font in fonts],str(build/'lvgl/lib/liblvgl.a'),'-o',str(build/'audit-wordclock')],build/'wordclock-compile.log')
    stdout=run([str(build/'audit-wordclock')]);records=[];glyphs=[]
    for line in stdout.splitlines():
        parts=line.split('\t')
        if parts[0]=='CLOCK':
            records.append(dict(language=parts[1],time=parts[2],truncated=bool(int(parts[3])),date_truncated=bool(int(parts[4])),
                raw_missing_glyphs=int(parts[5]),font_height=int(parts[6]),bounds=list(map(int,parts[7:11])),
                raw=parts[11],rendered=parts[12],date=parts[13],production_date=parts[14]))
        elif parts[0]=='GLYPH':glyphs.append(dict(codepoint='U+'+parts[1],font_height=int(parts[2]),present=bool(int(parts[3]))))
    failures=[r for r in records if not r['rendered']or r['truncated']or r['bounds'][1]<30 or r['bounds'][3]>171]
    index={(r['language'],r['time']):r['raw']for r in records};semantic=[]
    def expect(condition,description):
        if not condition:semantic.append(description)
    for code in LANGUAGES:
        if code in ['pir','bellazio']:continue  # Their introductions vary per minute intentionally.
        for hour in [0,1,2,12,23]:
            for first,second in [(0,1),(1,2),(14,15),(29,30),(44,45),(58,59)]:
                expect(index[code,f'{hour:02d}:{first:02d}']==index[code,f'{hour:02d}:{second:02d}'],f'{code} inconsistent five-minute bucket {hour}:{first}/{second}')
        expect(index[code,'23:59']==index[code,'00:00'],code+' midnight rounding mismatch')
        expect(index[code,'00:58']==index[code,'01:00'],code+' next-hour rounding mismatch')
    for key,fragment in [(('en','00:03'),'5 past twelve'),(('en','23:57'),'5 to twelve'),
                         (('es','01:15'),'Es la una'),(('es','12:45'),'Es la una'),
                         (('de','01:15'),'nach eins'),(('de','00:58'),'ein Uhr'),
                         (('eo','00:00'),'dek-dua'),(('eo','01:15'),'post la unua'),
                         (('eo','12:45'),'antaŭ la unua'),(('tlh','00:00'),"wa'maH cha' rep")]:
        expect(fragment in index[key],f'{key} expected {fragment!r}, got {index[key]!r}')
    metadata.update(scope='Actual14 word-clock/date functions + production HOME auto-fit. Native software only.',
                    ascii_pipeline=pipeline_ascii,clock_capacity=clock_capacity,date_capacity=date_capacity,
                    records=records,glyph_probes=glyphs,fit_failures=failures,semantic_boundary_failures=semantic)
    prefix=args.out_prefix;prefix.parent.mkdir(parents=True,exist_ok=True)
    prefix.with_suffix('.json').write_text(json.dumps(metadata,indent=2,ensure_ascii=False)+'\n')
    rows=['# Orologio: matrice nativa finita','',f"{len(records)} campioni:14lingue ×5ore ×13minuti. Pipeline ASCII attiva: {pipeline_ascii}.",'',
          f"Frasi vuote/troncate o fuoriHOME: {len(failures)}. Date troncate nel buffer{date_capacity}byte: {sum(r['date_truncated']for r in records)}.",
          f"Controlli semantici dei confini e delle correzioni: {len(semantic)} errori.",'',
          '| Lingua | Ora | Frase originale | Frase inviata al font | Font height | Bounds |','|---|---|---|---|---:|---|']
    for r in records:
        if r['time']in ['00:00','00:58','01:15','02:30','12:45','23:59']:
            rows.append('|'+ '|'.join([r['language'],r['time'],r['raw'],r['rendered'],str(r['font_height']),str(r['bounds'])])+'|')
    prefix.with_suffix('.md').write_text('\n'.join(rows)+'\n')
    print(f"Wordclock samples={len(records)} fit_failures={len(failures)} date_truncations={sum(r['date_truncated']for r in records)} -> {prefix}")

if __name__=='__main__':main()
