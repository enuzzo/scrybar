#!/usr/bin/env python3
"""Check source extraction and the durable native all-page QA manifest."""
import hashlib
import json
from pathlib import Path
import sys
import unittest
sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import render_display_quality_native as renderer
from render_mac_stats_native import function

class NativeSourceExtractionTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source=(ROOT/'scrybar.ino').read_text()
        cls.cpp,cls.themes,cls.meta=renderer.generated_source(
            cls.source,(ROOT/'config.h').read_text(),(ROOT/'src/ui_strings.h').read_text())

    def test_real_widget_bodies_are_extracted_without_layout_translation(self):
        for name in self.meta['extracted_functions']:
            original=function(self.source,name)
            body=original[original.index('{'):]
            self.assertIn(body,self.cpp,name)
        for name in ['initLvglInfoPanel','initLvglClockPanel','initLvglWeatherPanel',
                     'lvglInitFeedDeck','lvglInitNowPlayingUi','lvglInitTransitUi',
                     'lvglInitLaunchUi','lvglInitBambuUi','doomBlitTitlePicToCanvas']:
            self.assertIn(name,self.meta['extracted_functions'])

    def test_snapshot_is_identified_and_hardware_headers_are_not_compiled(self):
        self.assertEqual(self.meta['source_sha256'],hashlib.sha256(self.source.encode()).hexdigest())
        self.assertEqual(self.meta['native_unit_sha256'],hashlib.sha256(self.cpp.encode()).hexdigest())
        self.assertEqual(len(self.themes),7)
        self.assertEqual(len(set(renderer.PAGES)),10)
        for header in ['<WiFi.h>','<HTTPClient.h>','"secrets.h"','<Preferences.h>']:
            self.assertNotIn('#include '+header,self.cpp)
        self.assertIn('not gameplay',self.meta['doom_scope'])

    def test_public_info_fixture_is_explicit_and_anonymous(self):
        fixture=(ROOT/'tests/fixtures/display_quality_native.cpp').read_text()
        for value in ['ExampleNetwork','02:00:00:00:00:24','192.0.2.24:8080',
                      'https://example.invalid','ScryBar Info / Example']:
            self.assertIn(value,fixture)
        self.assertNotIn('WiFi.SSID()',fixture)
        self.assertNotIn('WiFi.macAddress()',fixture)

class FinalNativeMatrixTest(unittest.TestCase):
    def test_final_matrix_and_launch_header_bounds(self):
        path=ROOT/'screenshots/display-quality-r321/native/manifest.json'
        if not path.exists():self.skipTest('Render the native quality matrix first')
        data=json.loads(path.read_text())
        self.assertEqual(len(data['records']),70)
        self.assertEqual({r['page'] for r in data['records']},set(renderer.PAGES))
        self.assertEqual(len({r['theme'] for r in data['records']}),7)
        from PIL import Image
        for record in data['records']:
            with Image.open(path.parent/record['image']) as picture:
                self.assertEqual(picture.size,(640,172),record['image'])
            if record['page']=='LAUNCH':
                roles={r['role']:r['bounds'] for r in record['roles']}
                left,center,right=[roles['launch.header.'+name] for name in ['title','center','fetch']]
                self.assertLess(left[2],center[0],record['image'])
                self.assertLess(center[2],right[0],record['image'])

if __name__=='__main__':unittest.main()
