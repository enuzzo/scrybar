import homeDefault from '../assets/screenshots/home_weather_scrybar-default.png';
import homeCyber from '../assets/screenshots/home_weather_cyberpunk-2077.png';
import homeToxic from '../assets/screenshots/home_weather_toxic-candy.png';
import homeTokyo from '../assets/screenshots/home_weather_tokyo-transit.png';
import homeMono from '../assets/screenshots/home_weather_minimal-brutalist-mono.png';
import homeMint from '../assets/screenshots/home_weather_mint-protocol.png';
import homeCrt from '../assets/screenshots/home_weather_cathode-ray.png';
import rss from '../assets/screenshots/aux_rss.png';
import wiki from '../assets/screenshots/wiki_stream.png';
import music from '../assets/screenshots/now_playing.png';
import transit from '../assets/screenshots/timetable_board.png';
import launch from '../assets/screenshots/launch_countdown.png';
import stats from '../assets/screenshots/mac_stats_scrybar-default--live.png';
import bambu from '../assets/screenshots/bambu_lab.png';
import diagnostics from '../assets/screenshots/diagnostics-example-native.png';

export const themes = [
  {id:'scrybar-default',name:'Default',color:'#7551ff',image:homeDefault},
  {id:'cyberpunk-2077',name:'Cyberpunk',color:'#fcee0a',image:homeCyber},
  {id:'toxic-candy',name:'Toxic Candy',color:'#ff2bd6',image:homeToxic},
  {id:'tokyo-transit',name:'Tokyo Transit',color:'#00d1ff',image:homeTokyo},
  {id:'minimal-brutalist-mono',name:'Mono',color:'#ffffff',image:homeMono},
  {id:'mint-protocol',name:'Mint Protocol',color:'#58855c',image:homeMint},
  {id:'cathode-ray',name:'Cathode Ray',color:'#33ff33',image:homeCrt},
];
export const languages = ['English','Italiano','Français','Deutsch','Español','Português','Latina','Esperanto','Klingon','1337 Speak','Shakespearean','Valley Girl','Bellazio','Pirate'];

// Public view gallery. Doom is an optional extra, not a tenth gallery capture.
export const views = [
  {id:'home',name:'Clock + Weather',image:homeDefault,description:'The time in words, current conditions and a three-hour outlook. Fourteen clock languages, including a few less sensible ones.',note:'Device capture. Home preview follows the selected theme.'},
  {id:'rss',name:'RSS',image:rss,description:'Your own feeds in one place. Scan the QR code to finish an article on your phone.',note:'Device capture. Up to five feeds, one to eight articles each.'},
  {id:'wiki',name:'Wikipedia',image:wiki,description:'Featured stories, On This Day and random discoveries. A small detour for a curious desk.',note:'Device capture. Wikipedia has eight language options.'},
  {id:'music',name:'Now Playing',image:music,description:'Track, album art and progress from your Mac, without opening another window.',note:'Device capture. Requires ScryBar Companion on your LAN.'},
  {id:'transit',name:'Departures',image:transit,description:'A departure board for your chosen stop. Filter by destination and see when it is time to leave.',note:'Device capture. Powered by Transitous; not a route planner.'},
  {id:'launch',name:'Launches',image:launch,description:'The next launch countdown, weather at the pad and a QR link to the mission.',note:'Device capture. Launch information can change.'},
  {id:'stats',name:'Mac Stats',image:stats,description:'Mac activity and the last minute of network traffic, sent by Companion.',note:'Native firmware render with sample data.'},
  {id:'bambu',name:'Bambu Lab',image:bambu,description:'Print progress, layers, temperatures and AMS telemetry nearby. Open the A1/P1 camera when you want a closer look.',note:'Device capture during an A1 print. Requires Companion.'},
  {id:'diagnostics',name:'Diagnostics',image:diagnostics,description:'Connection, power and firmware information when you need to check the device.',note:'Native firmware render with sample network values and an example QR code.'},
];

export const statsImages = import.meta.glob<{default: import('astro').ImageMetadata}>('../assets/screenshots/mac_stats_*--live.png', {eager:true});
export function statsImageFor(theme:string){
  return statsImages[`../assets/screenshots/mac_stats_${theme}--live.png`].default;
}
