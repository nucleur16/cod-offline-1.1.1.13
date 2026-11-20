# CoDOL Classic bug bridge

This bot turns posts in the private `#mp-bugs` and `#zm-bugs` Discord forums into issues in [`nucleur16/codol_1.1.1.13_alpha_server`](https://github.com/nucleur16/codol_1.1.1.13_alpha_server/issues). It imports active and archived existing posts on first run, then watches new posts. The GitHub issue gets the original post text, attachment URLs, a link back to Discord, and `bug` plus `area:mp` or `area:zm` labels. Some Zombies posts are currently in `#mp-bugs`; the three clearly identifiable ones have initial labels in `area_overrides.json`, including one cross-mode report labeled both. Moderators can correct any other category with `/bug area`. A SQLite database prevents duplicate imports. If the database is lost, the bot checks issue bodies for its Discord thread marker before creating a replacement.

**Privacy:** GitHub Issues in this public repository are public. The first import copies private forum report text, reporter display names, and attachment URLs there. Review reports before starting the bot if any content should stay private.

GitHub Issues are the status source of truth. Closing or reopening an issue on GitHub posts a status update in its Discord thread within about two minutes. In a bug thread, moderators with Discord **Manage Threads** can use `/bug close` (Fixed or Not planned), `/bug reopen`, `/bug artifact`, `/bug note`, and `/bug area`; anyone who can see the thread can use `/bug link`. Closing a GitHub issue does not archive the Discord thread, so testers can still retest and discuss it.

## Install on the always-on PC

1. Install Python 3.10 or newer and copy this directory to the PC. Keep the PC online for live sync.
2. Create a Discord application and bot in the [Developer Portal](https://discord.com/developers/applications). Enable the **Message Content** privileged gateway intent. Install it into **CoDOL Classic** with the `bot` and `applications.commands` scopes. Give the bot access only to `#mp-bugs` and `#zm-bugs`, with **View Channel**, **Read Message History**, and **Send Messages in Threads**. The bot does not require Administrator.
3. Create a [fine-grained GitHub personal access token](https://github.com/settings/personal-access-tokens/new) limited to `nucleur16/codol_1.1.1.13_alpha_server` with repository **Issues: Read and write**. The account needs permission to manage labels; the bot creates `bug`, `area:mp`, and `area:zm` if absent. A GitHub App installation token with equivalent permission can also be used, but short-lived installation tokens need a refresh service.
4. Copy `.env.example` to `.env` in this directory and fill in the two tokens. Keep `.env` and `bug_bridge.sqlite3` private; both are ignored by Git. Back up the SQLite file periodically.
5. In PowerShell, from this directory, run the following one line at a time:

   ```powershell
   py -3 -m venv .venv
   .\.venv\Scripts\python.exe -m pip install -r requirements.txt
   .\.venv\Scripts\python.exe -m unittest test_core.py
   .\.venv\Scripts\python.exe bot.py
   ```

On the first run, leave `BUG_BRIDGE_BACKFILL=1`; watch the console for import errors and verify the issue links appear in both forums. Later starts remain safe because the mapping is durable. To disable scanning old posts on a new installation, set it to `0`.

For an unattended Windows deployment, run `bot.py` with an account that can read this directory using Task Scheduler at logon or startup. Set **Start in** to this directory and **Program** to the absolute path of `.venv\Scripts\python.exe`; use the absolute path of `bot.py` as the argument. Set Task Scheduler to restart the task on failure. Do not place tokens in task arguments or in a public repo.

## Report and artifact workflow

Ask testers to put their **build version or source commit**, client/server/launcher component, steps, expected result, actual result, and logs/screenshots in the original forum post. If they do not know the build, the issue records that it was not supplied; triage can ask for it in Discord.

Once a tester identifies the exact binary or build, a moderator runs `/bug artifact` in the post with the artifact/release/Actions URL, SHA-256 when available, and source commit. This adds a permanent GitHub issue comment and a pointer in Discord. A link to a GitHub Actions run identifies the run; the file SHA-256 identifies the tested bytes. Actions artifacts can expire, so prefer a stable release or package link when available. Use a second `/bug artifact` call for a fixed build and close the issue only after that build has been tested.

The existing `.github/workflows/build.yml` is inherited from CoD4x and does not currently provide a verified CoDOL release artifact for every test build. The bridge never guesses which binary a reporter used.
