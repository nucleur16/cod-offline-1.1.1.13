"""Sync CoDOL Classic forum bugs with GitHub Issues.

Run from this directory: python bot.py. Requires discord.py and a local .env.
"""

from __future__ import annotations

import asyncio
import json
import logging
import os
from pathlib import Path

import discord
from discord import app_commands
from discord.ext import commands

from core import GUILD_ID, MP_FORUM_ID, REPOSITORY, ZM_FORUM_ID, GitHub, Mappings, Report, artifact_comment


HERE = Path(__file__).resolve().parent
LOG = logging.getLogger("bug_bridge")


def load_env(path: Path):
    if not path.exists():
        return
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        os.environ.setdefault(key.strip(), value.strip().strip('"').strip("'"))


load_env(HERE / ".env")
guild_id = int(os.getenv("DISCORD_GUILD_ID", str(GUILD_ID)))
mp_forum_id = int(os.getenv("DISCORD_MP_FORUM_ID", str(MP_FORUM_ID)))
zm_forum_id = int(os.getenv("DISCORD_ZM_FORUM_ID", str(ZM_FORUM_ID)))
area_labels = {mp_forum_id: ("MP", "area:mp"), zm_forum_id: ("Zombies", "area:zm")}
area_overrides = json.loads((HERE / "area_overrides.json").read_text(encoding="utf-8"))
discord_token = os.getenv("DISCORD_BOT_TOKEN", "")
github_token = os.getenv("GITHUB_TOKEN", "")
if not discord_token or not github_token:
    raise SystemExit("Set DISCORD_BOT_TOKEN and GITHUB_TOKEN in tools/discord_bug_bridge/.env")

github = GitHub(github_token, os.getenv("GITHUB_REPOSITORY", REPOSITORY))
db_path = Path(os.getenv("BUG_BRIDGE_DB", "bug_bridge.sqlite3"))
if not db_path.is_absolute():
    db_path = HERE / db_path
db = Mappings(db_path)

intents = discord.Intents.default()
intents.guilds = True
intents.messages = True
intents.message_content = True


class BugBot(commands.Bot):
    def __init__(self):
        super().__init__(command_prefix=commands.when_mentioned, intents=intents)
        self.import_lock = asyncio.Lock()
        self.started = False

    async def setup_hook(self):
        self.tree.add_command(bug_group, guild=discord.Object(id=guild_id))
        await self.tree.sync(guild=discord.Object(id=guild_id))

    async def on_ready(self):
        LOG.info("Ready in Discord guild %s", guild_id)
        if not self.started:
            self.started = True
            asyncio.create_task(self.initialize())

    async def initialize(self):
        try:
            await asyncio.to_thread(github.ensure_labels)
            asyncio.create_task(self.watch_github())
            if os.getenv("BUG_BRIDGE_BACKFILL", "1") == "1":
                await self.backfill()
        except Exception:
            LOG.exception("Initialization failed; no live sync is running")

    async def backfill(self):
        guild = self.get_guild(guild_id)
        if guild is None:
            raise RuntimeError("Bot is not installed in the configured guild")
        for forum_id in area_labels:
            forum = guild.get_channel(forum_id) or await self.fetch_channel(forum_id)
            if not isinstance(forum, discord.ForumChannel):
                raise RuntimeError(f"Configured channel {forum_id} is not a forum")
            active = [thread for thread in guild.threads if thread.parent_id == forum_id]
            for thread in active:
                await self.import_safely(thread)
            async for thread in forum.archived_threads(limit=None):
                await self.import_safely(thread)

    async def import_safely(self, thread: discord.Thread):
        try:
            await self.import_thread(thread)
        except Exception:
            LOG.exception("Could not import Discord thread %s", thread.id)

    async def on_thread_create(self, thread: discord.Thread):
        if thread.guild.id == guild_id and thread.parent_id in area_labels:
            await self.import_safely(thread)

    async def import_thread(self, thread: discord.Thread):
        if thread.parent_id not in area_labels:
            return
        async with self.import_lock:
            row = db.get(thread.id)
            if row:
                if not row[5]:
                    await self.announce(thread, row[3])
                return
            starter = None
            async for message in thread.history(limit=1, oldest_first=True):
                starter = message
            if starter is None:
                LOG.warning("No starter message in thread %s", thread.id)
                return
            report = Report(
                thread_id=thread.id,
                forum_id=thread.parent_id,
                title=thread.name,
                author=starter.author.display_name,
                content=starter.content,
                attachments=tuple((item.filename, item.url) for item in starter.attachments),
                area_name=", ".join("MP" if name == "area:mp" else "Zombies" for name in selected_areas(thread)),
                guild_id=guild_id,
            )
            issue = await asyncio.to_thread(github.find_thread_issue, thread.id)
            if issue is None:
                issue = await asyncio.to_thread(github.create_issue, thread.name, report.body, selected_areas(thread))
                LOG.info("Created issue #%s from thread %s", issue["number"], thread.id)
                await asyncio.sleep(1)  # Avoid GitHub secondary rate limits during backfill.
            db.upsert(thread.id, thread.parent_id, issue)
            await self.announce(thread, issue["html_url"])

    async def announce(self, thread: discord.Thread, issue_url: str):
        try:
            await thread.send(f"Tracked as GitHub Issue: {issue_url}\nUse `/bug link` to find it later.")
            db.announced(thread.id)
        except (discord.Forbidden, discord.HTTPException):
            LOG.warning("Issue linked but could not post in thread %s", thread.id)

    async def watch_github(self):
        while not self.is_closed():
            await asyncio.sleep(120)
            for row in db.all():
                thread_id, _forum_id, number, issue_url, previous_state, _announced = row
                try:
                    issue = await asyncio.to_thread(github.get_issue, number)
                    state = issue["state"]
                    if state != previous_state:
                        db.state(thread_id, state)
                        thread = self.get_channel(thread_id) or await self.fetch_channel(thread_id)
                        await thread.send(f"GitHub Issue #{number} is now **{state}**: {issue_url}")
                except Exception:
                    LOG.exception("Could not sync GitHub issue #%s", number)


bot = BugBot()
bug_group = app_commands.Group(name="bug", description="Track forum bug reports in GitHub Issues")


def configured_thread(interaction: discord.Interaction) -> discord.Thread | None:
    channel = interaction.channel
    if (
        interaction.guild_id == guild_id
        and isinstance(channel, discord.Thread)
        and channel.parent_id in area_labels
    ):
        return channel
    return None


def can_triage(interaction: discord.Interaction) -> bool:
    member = interaction.user
    return (
        isinstance(member, discord.Member)
        and isinstance(interaction.channel, discord.Thread)
        and interaction.channel.permissions_for(member).manage_threads
    )


def selected_areas(thread: discord.Thread) -> list[str]:
    return area_overrides.get(str(thread.id), [area_labels[thread.parent_id][1]])


@bug_group.command(name="link", description="Show the GitHub Issue for this bug thread")
async def link(interaction: discord.Interaction):
    thread = configured_thread(interaction)
    if thread is None:
        await interaction.response.send_message("Run this inside #mp-bugs or #zm-bugs.", ephemeral=True)
        return
    row = db.get(thread.id)
    if row is None:
        await interaction.response.send_message("This post has not been imported yet. Ask a moderator to check the bot logs.", ephemeral=True)
        return
    await interaction.response.send_message(f"GitHub Issue #{row[2]} ({row[4]}): {row[3]}", ephemeral=True)


@bug_group.command(name="close", description="Close this bug on GitHub")
@app_commands.describe(reason="Why this bug is being closed")
@app_commands.choices(reason=[
    app_commands.Choice(name="Fixed", value="completed"),
    app_commands.Choice(name="Not planned", value="not_planned"),
])
async def close(interaction: discord.Interaction, reason: app_commands.Choice[str]):
    thread = configured_thread(interaction)
    if thread is None or not can_triage(interaction):
        await interaction.response.send_message("Only moderators can close bugs inside a bug forum thread.", ephemeral=True)
        return
    row = db.get(thread.id)
    if row is None:
        await interaction.response.send_message("No GitHub Issue is linked to this post yet.", ephemeral=True)
        return
    await interaction.response.defer(ephemeral=True)
    try:
        issue = await asyncio.to_thread(github.set_issue_state, row[2], "closed", reason.value)
        db.state(thread.id, issue["state"])
        await thread.send(f"Bug closed ({reason.name}) by {interaction.user.mention}: {row[3]}")
        await interaction.followup.send(f"Closed GitHub Issue #{row[2]}.", ephemeral=True)
    except Exception:
        LOG.exception("Could not close issue #%s", row[2])
        await interaction.followup.send("GitHub or Discord update failed. Check the issue and bot logs before retrying.", ephemeral=True)


@bug_group.command(name="reopen", description="Reopen this bug on GitHub")
async def reopen(interaction: discord.Interaction):
    thread = configured_thread(interaction)
    if thread is None or not can_triage(interaction):
        await interaction.response.send_message("Only moderators can reopen bugs inside a bug forum thread.", ephemeral=True)
        return
    row = db.get(thread.id)
    if row is None:
        await interaction.response.send_message("No GitHub Issue is linked to this post yet.", ephemeral=True)
        return
    await interaction.response.defer(ephemeral=True)
    try:
        issue = await asyncio.to_thread(github.set_issue_state, row[2], "open", "reopened")
        db.state(thread.id, issue["state"])
        await thread.send(f"Bug reopened by {interaction.user.mention}: {row[3]}")
        await interaction.followup.send(f"Reopened GitHub Issue #{row[2]}.", ephemeral=True)
    except Exception:
        LOG.exception("Could not reopen issue #%s", row[2])
        await interaction.followup.send("GitHub or Discord update failed. Check the issue and bot logs before retrying.", ephemeral=True)


@bug_group.command(name="artifact", description="Link the exact tested build to this bug")
@app_commands.describe(kind="Which component was tested", url="HTTPS download, release, or Actions run URL", sha256="Optional 64-character file SHA-256", commit="Optional 7-40 character source commit SHA")
@app_commands.choices(kind=[
    app_commands.Choice(name="Client", value="client"),
    app_commands.Choice(name="Server", value="server"),
    app_commands.Choice(name="Launcher", value="launcher"),
    app_commands.Choice(name="Other", value="other"),
])
async def artifact(interaction: discord.Interaction, kind: app_commands.Choice[str], url: str, sha256: str = "", commit: str = ""):
    thread = configured_thread(interaction)
    if thread is None or not can_triage(interaction):
        await interaction.response.send_message("Only moderators can link artifacts inside a bug forum thread.", ephemeral=True)
        return
    row = db.get(thread.id)
    if row is None:
        await interaction.response.send_message("No GitHub Issue is linked to this post yet.", ephemeral=True)
        return
    try:
        body = artifact_comment(kind.value, url, sha256, commit, f"https://discord.com/channels/{guild_id}/{thread.id}", interaction.user.display_name)
    except ValueError as exc:
        await interaction.response.send_message(str(exc), ephemeral=True)
        return
    await interaction.response.defer(ephemeral=True)
    try:
        await asyncio.to_thread(github.comment, row[2], body)
        await thread.send(f"Build evidence linked to GitHub Issue #{row[2]}: {url}")
        await interaction.followup.send("Artifact evidence recorded on the GitHub Issue.", ephemeral=True)
    except Exception:
        LOG.exception("Could not add artifact to issue #%s", row[2])
        await interaction.followup.send("GitHub or Discord update failed. Check the issue and bot logs before retrying.", ephemeral=True)


@bug_group.command(name="note", description="Add a triage or retest note to the GitHub Issue")
@app_commands.describe(text="Finding, reproduction detail, or retest result")
async def note(interaction: discord.Interaction, text: str):
    thread = configured_thread(interaction)
    if thread is None or not can_triage(interaction):
        await interaction.response.send_message("Only moderators can add issue notes inside a bug forum thread.", ephemeral=True)
        return
    row = db.get(thread.id)
    if row is None:
        await interaction.response.send_message("No GitHub Issue is linked to this post yet.", ephemeral=True)
        return
    if len(text.strip()) < 5:
        await interaction.response.send_message("Enter a useful note of at least five characters.", ephemeral=True)
        return
    await interaction.response.defer(ephemeral=True)
    body = f"Triage note from [Discord thread](https://discord.com/channels/{guild_id}/{thread.id}) by {interaction.user.display_name.replace('@', '@\u200b')}:\n\n{text.replace('@', '@\u200b')}"
    try:
        await asyncio.to_thread(github.comment, row[2], body)
        await interaction.followup.send(f"Added note to GitHub Issue #{row[2]}.", ephemeral=True)
    except Exception:
        LOG.exception("Could not add note to issue #%s", row[2])
        await interaction.followup.send("GitHub update failed. Check the issue and bot logs before retrying.", ephemeral=True)


@bug_group.command(name="area", description="Correct the MP or Zombies labels for this bug")
@app_commands.describe(area="The game area affected")
@app_commands.choices(area=[
    app_commands.Choice(name="Multiplayer", value="area:mp"),
    app_commands.Choice(name="Zombies", value="area:zm"),
    app_commands.Choice(name="Both", value="both"),
])
async def area(interaction: discord.Interaction, area: app_commands.Choice[str]):
    thread = configured_thread(interaction)
    if thread is None or not can_triage(interaction):
        await interaction.response.send_message("Only moderators can change the area inside a bug forum thread.", ephemeral=True)
        return
    row = db.get(thread.id)
    if row is None:
        await interaction.response.send_message("No GitHub Issue is linked to this post yet.", ephemeral=True)
        return
    labels = ["area:mp", "area:zm"] if area.value == "both" else [area.value]
    await interaction.response.defer(ephemeral=True)
    try:
        await asyncio.to_thread(github.set_issue_area, row[2], labels)
        await interaction.followup.send(f"Updated GitHub Issue #{row[2]} to {area.name}.", ephemeral=True)
    except Exception:
        LOG.exception("Could not change area on issue #%s", row[2])
        await interaction.followup.send("GitHub update failed. Check the issue and bot logs before retrying.", ephemeral=True)


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(name)s: %(message)s")
    bot.run(discord_token, log_handler=None)
