#!/usr/bin/env python3
"""Focused parser/format checks for the source-driven native quality audit."""
import pathlib
import sys
import unittest

sys.dont_write_bytecode = True
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import audit_ui_quality_native as audit


class AuditInputTests(unittest.TestCase):
    def test_format_contracts_and_literal_percent(self):
        self.assertEqual(audit.format_signature("Wind %.0f knots"), (["float"], []))
        self.assertEqual(audit.format_signature("In 3h, matey: %s"), (["s"], []))
        self.assertEqual(audit.format_signature("100%% ready"), ([], []))
        self.assertEqual(audit.format_signature("%08.2f %s"), (["float", "s"], []))

    def test_unsafe_or_unexpected_formats_do_not_reach_formatter(self):
        for value in ("%*s", "%1$s", "%", "%q"):
            self.assertTrue(audit.format_signature(value)[1], value)
        # Syntactically valid but wrong for a weather fmt's one-float contract.
        self.assertNotEqual(audit.format_signature("Wind %n")[0], ["float"])
        self.assertNotEqual(audit.format_signature("Wind %s")[0], ["float"])

    def test_call_parser_preserves_nested_calls_strings_and_comments(self):
        source = r'''skip(); target(widget, mix(0xFF, fn("x,y)", 3)),
            /* comma, ) should be ignored */ pressed ? colorA : colorB);'''
        args = audit.arguments(source, "target")
        self.assertEqual(len(args), 3)
        self.assertEqual(args[0], "widget")
        self.assertEqual(args[1], 'mix(0xFF, fn("x,y)", 3))')
        self.assertTrue(args[2].endswith("pressed ? colorA : colorB"))

    def test_call_parser_handles_initializer_and_lambda_commas(self):
        args = audit.arguments("apply(x, Pair{1,2}, [](int a,int b){return a+b;});", "apply")
        self.assertEqual(args, ["x", "Pair{1,2}", "[](int a,int b){return a+b;}"])

    def test_missing_or_unbalanced_calls_fail_explicitly(self):
        with self.assertRaises(ValueError): audit.arguments("another(x)", "wanted")
        with self.assertRaises(ValueError): audit.arguments("wanted(x, nested(1)", "wanted")

    def test_visible_helper_resolves_last_text_opacity_write(self):
        source = """static void lvglForceLabelVisible(lv_obj_t *obj) {
          lvglSetLocalNumberIfChanged(obj, LV_STYLE_TEXT_OPA, LV_OPA_COVER, 0);
        }"""
        direct = "lv_obj_set_style_text_opa(ui.label, LV_OPA_70, 0);"
        self.assertEqual(audit.effective_text_opacity(source, direct, "ui.label"), "LV_OPA_70")
        self.assertEqual(audit.effective_text_opacity(source, direct+"lvglForceLabelVisible(ui.label);", "ui.label"), "LV_OPA_COVER")
        self.assertEqual(audit.effective_text_opacity(source, "lvglForceLabelVisible(ui.label);"+direct, "ui.label"), "LV_OPA_70")

    def test_geometry_extracts_only_dependencies_in_order(self):
        source = """const int16_t cardW = cW;
        const int16_t unused = 999;
        constexpr int16_t pad = 8;
        const int16_t leftW = cardW - pad * 2;"""
        code = audit.geometry(source, ["leftW"])
        self.assertNotIn("unused", code)
        self.assertLess(code.index("cardW ="), code.index("leftW ="))
        self.assertLess(code.index("pad ="), code.index("leftW ="))

    def test_rendered_surface_crosscheck_reports_one_changed_pixel(self):
        samples={"themes":{"test":{}}}
        roles=[]
        for page,key,name in (("NOWPLAYING","header","nowplaying.headerTime.native_fixture"),
                              ("NOWPLAYING","body","nowplaying.album.native_fixture"),
                              ("LAUNCH","hero","launch.weather"),
                              ("TRANSIT","normal_row","transit.delay.late"),
                              ("TRANSIT","odd_row","transit.delay.late.odd")):
            samples["themes"]["test"].setdefault(page,{"samples":{}})["samples"][key]={"x":20,"y":30,"rgb":[222,234,222]}
            roles.append(dict(theme="test",role=name,rgb565_bg="#DEEADE"))
        checks=audit.compare_native_surfaces(roles,samples)
        self.assertEqual(len(checks),5)
        self.assertTrue(all(c["matches"] for c in checks))
        samples["themes"]["test"]["TRANSIT"]["samples"]["odd_row"]["rgb"][0]=221
        self.assertEqual(sum(not c["matches"] for c in audit.compare_native_surfaces(roles,samples)),1)

    def test_actual_source_extraction_does_not_embed_duplicate_palette(self):
        source = (audit.ROOT / "scrybar.ino").read_text()
        strings = (audit.ROOT / "src/ui_strings.h").read_text()
        config = (audit.ROOT / "config.h").read_text()
        cpp, metadata = audit.generated_source(source, strings, config)
        self.assertEqual(len(metadata["theme_ids"]), 7)
        self.assertEqual(len(metadata["languages"]), 14)
        self.assertIn("kUiLang_pir.windFmt", cpp)
        self.assertIn("kUiLang_bellazio.touchToCloseAnywhere", cpp)
        self.assertIn("kUiLang_sha.rssSyncing", cpp)
        self.assertIn(audit.function(source, "lvglResolvedAuxButtonText"), cpp)
        self.assertIn(audit.function(source, "lvglLaunchWeatherAccent"), cpp)
        self.assertTrue(any(r["role"] == "launch.header.fetch" for r in metadata["roles"]))
        roles = {r["role"]:r for r in metadata["roles"]}
        self.assertEqual(roles["launch.weather"]["background_expression"], "launchHeroBg")
        self.assertIn("auditBlendLvgl", roles["launch.qr.affordance"]["foreground_expression"])
        self.assertIn("lv_color_mix", cpp)
        self.assertIn("lv_color_to32", cpp)
        self.assertIn("transit.delay.late.odd", roles)
        self.assertIn("nowplaying.album.midgray", roles)
        self.assertIn("nowplaying.dot.live.midgray", roles)
        self.assertIn("headerSurface", roles["nowplaying.headerTime.midgray"]["context"])


if __name__ == "__main__":
    unittest.main()
