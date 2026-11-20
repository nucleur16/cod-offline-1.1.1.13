"""Survival map availability must require the data the selected mode consumes."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('catalogue', Path(__file__).parents[1] / 'tools/survival_catalogue.py')
catalogue = importlib.util.module_from_spec(spec)
spec.loader.exec_module(catalogue)

class SurvivalCatalogueTests(unittest.TestCase):
    def test_requires_installed_archive_and_complete_authored_placements(self):
        with tempfile.TemporaryDirectory() as tmp:
            game = Path(tmp)
            files = {
                'client_shipRetail/chinese/common_gametype_survival.ff': '',
                'main/sh1x_rawfiles/maps/mp/gametypes/survival.gsc': 'main() {}',
                'main/sh1x_rawfiles/maps/mp/gametypes/_survival_levels.gsc': 'case "mp_fav_tropical":',
                'main/sh1x_rawfiles/maps/mp/survival/mp_fav_tropical.gsc': 'level.survival_player_starts[ level.survival_leaders[ level.survival_kiosk_spots[',
            }
            for name, text in files.items():
                p = game / name
                p.parent.mkdir(parents=True, exist_ok=True)
                p.write_text(text)
            self.assertEqual(catalogue.available_maps(game), [])
            archive = game / 'client_shipRetail/chinese/mp_fav_tropical.ff'
            archive.touch()
            self.assertEqual(catalogue.available_maps(game), ['mp_fav_tropical'])
            (game / 'main/sh1x_rawfiles/maps/mp/survival/mp_fav_tropical.gsc').write_text('level.survival_player_starts[')
            self.assertEqual(catalogue.available_maps(game), [])
            (game / 'client_shipRetail/chinese/common_gametype_survival.ff').unlink()
            self.assertEqual(catalogue.available_maps(game), [])

if __name__ == '__main__':
    unittest.main()
