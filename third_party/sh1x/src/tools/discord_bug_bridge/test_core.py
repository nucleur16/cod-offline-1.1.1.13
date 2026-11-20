import tempfile
import unittest
from pathlib import Path

from core import MP_FORUM_ID, ZM_FORUM_ID, GitHub, Mappings, Report, artifact_comment


class ReportTests(unittest.TestCase):
    def test_report_keeps_source_and_area(self):
        report = Report(123, ZM_FORUM_ID, "Trap fails", "Tester@all", "Repro: @team\nBuild: unknown", (("screen.png", "https://cdn.discordapp.com/a.png"),))
        self.assertIn("<!-- discord-thread-id:123 -->", report.body)
        self.assertIn("**Area:** Zombies", report.body)
        self.assertIn("https://discord.com/channels/1529892888228659388/123", report.body)
        self.assertIn("https://cdn.discordapp.com/a.png", report.body)
        self.assertNotIn("@team", report.body)

    def test_mp_area(self):
        self.assertIn("**Area:** MP", Report(1, MP_FORUM_ID, "a", "b", "c", ()).body)

    def test_artifact_validation(self):
        value = artifact_comment("server", "https://github.com/a/b/actions/runs/1", "a" * 64, "abc1234", "https://discord.com/channels/1/2", "Mod")
        self.assertIn("`" + "a" * 64 + "`", value)
        with self.assertRaises(ValueError):
            artifact_comment("server", "http://example.com", "", "", "x", "y")
        with self.assertRaises(ValueError):
            artifact_comment("server", "https://example.com", "bad", "", "x", "y")


class MappingTests(unittest.TestCase):
    def test_upsert_does_not_lose_announcement(self):
        with tempfile.TemporaryDirectory() as temp:
            db = Mappings(Path(temp) / "bridge.sqlite3")
            issue = {"number": 7, "html_url": "https://github.com/a/b/issues/7", "state": "open"}
            db.upsert(123, MP_FORUM_ID, issue)
            db.announced(123)
            db.upsert(123, MP_FORUM_ID, issue)
            self.assertEqual((7, "open", 1), (db.get(123)[2], db.get(123)[4], db.get(123)[5]))
            db.state(123, "closed")
            self.assertEqual("closed", db.get(123)[4])
            self.assertEqual(1, len(db.all()))
            db.close()


class GitHubTests(unittest.TestCase):
    def test_area_change_preserves_other_labels_and_body(self):
        client = GitHub("test")
        calls = []

        def request(method, path, payload=None):
            calls.append((method, path, payload))
            if method == "GET":
                return {
                    "labels": [{"name": "bug"}, {"name": "priority:high"}, {"name": "area:mp"}],
                    "body": "<!-- discord-thread-id:123 -->\n- **Area:** MP\nReproduction",
                }
            return payload

        client.request = request
        client.set_issue_area(7, ["area:zm"])
        self.assertEqual(("PATCH", "/issues/7"), calls[1][:2])
        self.assertEqual(["bug", "priority:high", "area:zm"], calls[1][2]["labels"])
        self.assertIn("**Area:** Zombies", calls[1][2]["body"])


if __name__ == "__main__":
    unittest.main()
