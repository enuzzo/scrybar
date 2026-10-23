#!/usr/bin/env python3
"""Assert the finite native word-clock artifact covers its actual contract."""
import json
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]

class WordClockNativeTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data=json.loads((ROOT/'docs/design/2026-10-23-display-quality/wordclock.json').read_text())

    def test_boundary_matrix_is_complete_and_fits_real_home(self):
        records=self.data['records']
        self.assertEqual(len(records),910)
        self.assertEqual(len({(r['language'],r['time'])for r in records}),910)
        self.assertEqual(len({r['language']for r in records}),14)
        self.assertEqual(self.data['fit_failures'],[])
        self.assertEqual(self.data['semantic_boundary_failures'],[])
        for r in records:
            self.assertTrue(r['raw'] and r['rendered'] and r['date'])
            self.assertFalse(r['truncated'],r)
            self.assertFalse(r['date_truncated'],r)
            self.assertEqual(r['raw_missing_glyphs'],0,r)
            self.assertLess(len(r['raw'].encode()),self.data['clock_capacity'])
            self.assertLess(len(r['date'].encode()),self.data['date_capacity'])

    def test_internal_unicode_is_retained_and_native_fonts_cover_accents(self):
        self.assertFalse(self.data['ascii_pipeline'])
        self.assertEqual(len(self.data['glyph_probes']),12)
        self.assertTrue(all(p['present']for p in self.data['glyph_probes']))
        records={(r['language'],r['time']):r for r in self.data['records']}
        self.assertIn('È',records['it','01:15']['rendered'])
        self.assertIn('Venerdì',records['it','01:15']['production_date'])
        self.assertIn('zwölf',records['de','00:00']['rendered'])
        self.assertIn('antaŭ',records['eo','12:45']['rendered'])
        self.assertIn('Year of Our Plunder 2026',records['pir','00:00']['production_date'])

if __name__=='__main__':unittest.main()
