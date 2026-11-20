# CoDOL Classic: owner setup

Please run the included bridge on the always-on PC. It will import **all existing and future** bug posts from `#mp-bugs` and `#zm-bugs` into GitHub Issues. It copies the starter post, reporter display name, and attachment links from these private Discord forums into the **public** repository. The `area:mp` and `area:zm` labels identify the game area. The current `#zm-bugs` forum is empty; three visible Zombies/cross-mode posts in `#mp-bugs` have explicit starting labels in `area_overrides.json`.

1. Create a Discord application and bot at <https://discord.com/developers/applications>. Turn on **Message Content Intent** in Bot settings. Install it in guild `1529892888228659388` (CoDOL Classic) with `bot` and `applications.commands` scopes. In the two bug forums, grant only **View Channel**, **Read Message History**, and **Send Messages in Threads**. Give moderators **Manage Threads** in those forums so they can use triage commands. No Administrator permission is needed for the bot.
2. Create a fine-grained GitHub token limited to `nucleur16/codol_1.1.1.13_alpha_server`, with **Issues: Read and write**. The token owner must be able to manage issue labels. Never send either token to me or another chat; store both in the `.env` file on the always-on PC only.
3. Copy `.env.example` to `.env`, fill in the two tokens, and run the four PowerShell commands in `README.md` under **Install on the always-on PC**. Keep `BUG_BRIDGE_BACKFILL=1` for the first run. The bot creates the GitHub labels and imports old posts automatically. Check its console for errors and verify issue links in both Discord forums.
4. Leave the bot running through Task Scheduler. Back up `bug_bridge.sqlite3`. It is the local thread-to-issue mapping; the bot can also recover mappings from issue body markers if the database is lost.

Moderators use `/bug link`, `/bug area`, `/bug artifact`, `/bug note`, `/bug close`, and `/bug reopen` inside a report thread. GitHub closure and reopening are reflected back in Discord within about two minutes. Build evidence should contain the exact artifact URL, source commit, and SHA-256 of the tested binary when available.

The detailed setup and troubleshooting notes are in `README.md` in the same folder.
