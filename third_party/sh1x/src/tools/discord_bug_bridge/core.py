"""GitHub API and durable Discord-thread/issue mapping for the bug bridge."""

from __future__ import annotations

import json
import re
import sqlite3
from dataclasses import dataclass
from pathlib import Path
from urllib.error import HTTPError, URLError
from urllib.parse import quote
from urllib.request import Request, urlopen


REPOSITORY = "nucleur16/codol_1.1.1.13_alpha_server"
GUILD_ID = 1529892888228659388
MP_FORUM_ID = 1552066327944896614
ZM_FORUM_ID = 1554985439167774793
AREAS = {MP_FORUM_ID: ("MP", "area:mp"), ZM_FORUM_ID: ("Zombies", "area:zm")}
MARKER = "discord-thread-id:"
SHA256 = re.compile(r"^[0-9a-fA-F]{64}$")
COMMIT = re.compile(r"^[0-9a-fA-F]{7,40}$")


class GitHubError(RuntimeError):
    pass


class GitHub:
    def __init__(self, token: str, repository: str = REPOSITORY):
        if not re.fullmatch(r"[\w.-]+/[\w.-]+", repository):
            raise ValueError("GITHUB_REPOSITORY must be owner/repo")
        self.repository = repository
        self.base = f"https://api.github.com/repos/{repository}"
        self.token = token

    def request(self, method: str, path: str, payload: dict | None = None):
        data = json.dumps(payload).encode("utf-8") if payload is not None else None
        request = Request(
            self.base + path,
            data=data,
            method=method,
            headers={
                "Accept": "application/vnd.github+json",
                "Authorization": f"Bearer {self.token}",
                "Content-Type": "application/json",
                "User-Agent": "codol-discord-bug-bridge",
                "X-GitHub-Api-Version": "2022-11-28",
            },
        )
        try:
            with urlopen(request, timeout=25) as response:
                body = response.read()
                return json.loads(body) if body else None
        except HTTPError as exc:
            detail = exc.read(1000).decode("utf-8", "replace")
            raise GitHubError(f"GitHub {method} {path}: HTTP {exc.code}: {detail}") from exc
        except URLError as exc:
            raise GitHubError(f"GitHub {method} {path}: {exc.reason}") from exc

    def ensure_labels(self):
        for name, color, description in (
            ("bug", "d73a4a", "Confirmed or reported defect"),
            ("area:mp", "1d76db", "Multiplayer"),
            ("area:zm", "8957e5", "Zombies"),
        ):
            try:
                self.request("GET", "/labels/" + quote(name, safe=""))
            except GitHubError as exc:
                if "HTTP 404:" not in str(exc):
                    raise
                self.request("POST", "/labels", {"name": name, "color": color, "description": description})

    def find_thread_issue(self, thread_id: int):
        marker = f"<!-- {MARKER}{thread_id} -->"
        page = 1
        while True:
            items = self.request("GET", f"/issues?state=all&per_page=100&page={page}")
            for issue in items:
                if "pull_request" not in issue and marker in (issue.get("body") or ""):
                    return issue
            if len(items) < 100:
                return None
            page += 1

    def create_issue(self, title: str, body: str, labels: list[str]):
        return self.request("POST", "/issues", {"title": title[:256], "body": body, "labels": ["bug", *labels]})

    def get_issue(self, number: int):
        return self.request("GET", f"/issues/{number}")

    def set_issue_state(self, number: int, state: str, reason: str):
        return self.request("PATCH", f"/issues/{number}", {"state": state, "state_reason": reason})

    def comment(self, number: int, body: str):
        return self.request("POST", f"/issues/{number}/comments", {"body": body})

    def set_issue_area(self, number: int, areas: list[str]):
        issue = self.get_issue(number)
        labels = [item["name"] for item in issue["labels"] if item["name"] not in {"area:mp", "area:zm"}]
        labels.extend(areas)
        area_name = ", ".join("MP" if name == "area:mp" else "Zombies" for name in areas)
        body = re.sub(r"(?m)^- \*\*Area:\*\* .*?$", f"- **Area:** {area_name}", issue.get("body") or "", count=1)
        return self.request("PATCH", f"/issues/{number}", {"labels": labels, "body": body})


class Mappings:
    def __init__(self, path: str | Path):
        self.connection = sqlite3.connect(path)
        self.connection.execute("""
            CREATE TABLE IF NOT EXISTS mappings (
                thread_id INTEGER PRIMARY KEY,
                forum_id INTEGER NOT NULL,
                issue_number INTEGER NOT NULL UNIQUE,
                issue_url TEXT NOT NULL,
                state TEXT NOT NULL,
                announced INTEGER NOT NULL DEFAULT 0
            )
        """)
        self.connection.commit()

    def close(self):
        self.connection.close()

    def get(self, thread_id: int):
        return self.connection.execute(
            "SELECT thread_id,forum_id,issue_number,issue_url,state,announced FROM mappings WHERE thread_id=?",
            (thread_id,),
        ).fetchone()

    def all(self):
        return self.connection.execute(
            "SELECT thread_id,forum_id,issue_number,issue_url,state,announced FROM mappings"
        ).fetchall()

    def upsert(self, thread_id: int, forum_id: int, issue: dict):
        self.connection.execute("""
            INSERT INTO mappings (thread_id, forum_id, issue_number, issue_url, state)
            VALUES (?, ?, ?, ?, ?)
            ON CONFLICT(thread_id) DO UPDATE SET
              issue_number=excluded.issue_number, issue_url=excluded.issue_url,
              state=excluded.state
        """, (thread_id, forum_id, issue["number"], issue["html_url"], issue["state"]))
        self.connection.commit()

    def state(self, thread_id: int, state: str):
        self.connection.execute("UPDATE mappings SET state=? WHERE thread_id=?", (state, thread_id))
        self.connection.commit()

    def announced(self, thread_id: int):
        self.connection.execute("UPDATE mappings SET announced=1 WHERE thread_id=?", (thread_id,))
        self.connection.commit()


@dataclass(frozen=True)
class Report:
    thread_id: int
    forum_id: int
    title: str
    author: str
    content: str
    attachments: tuple[tuple[str, str], ...]
    area_name: str = ""
    guild_id: int = GUILD_ID

    @property
    def url(self):
        return f"https://discord.com/channels/{self.guild_id}/{self.thread_id}"

    @property
    def body(self):
        area = self.area_name or AREAS[self.forum_id][0]
        # Prevent reporter text from pinging GitHub users/teams.
        content = self.content.replace("@", "@\u200b").strip() or "No text supplied. See the Discord post."
        attachments = "\n".join(
            f"- [{name.replace('[', '').replace(']', '')}]({url})" for name, url in self.attachments
        ) or "None"
        return (
            f"<!-- {MARKER}{self.thread_id} -->\n"
            f"## Report\n\n"
            f"- **Area:** {area}\n"
            f"- **Discord thread:** {self.url}\n"
            f"- **Reporter:** {self.author.replace('@', '@\u200b').replace(chr(10), ' ')}\n"
            f"- **Build tested:** Not supplied; ask reporter for the release/commit and binary SHA-256.\n\n"
            f"### Original report\n\n{content}\n\n"
            f"### Attachments\n\n{attachments}\n\n"
            f"### Artifact evidence\n\nUse `/bug artifact` in the Discord thread to add a build URL, commit, and SHA-256 as an issue comment.\n"
        )


def artifact_comment(kind: str, url: str, sha256: str, commit: str, thread_url: str, reporter: str):
    if kind not in {"client", "server", "launcher", "other"}:
        raise ValueError("kind must be client, server, launcher, or other")
    if not url.startswith("https://") or any(c.isspace() for c in url):
        raise ValueError("artifact URL must be a single HTTPS URL")
    if sha256 and not SHA256.fullmatch(sha256):
        raise ValueError("SHA-256 must be exactly 64 hexadecimal characters")
    if commit and not COMMIT.fullmatch(commit):
        raise ValueError("commit must be a 7-40 character hexadecimal SHA")
    return (
        f"## Build evidence ({kind})\n\n"
        f"- Artifact: {url}\n"
        f"- SHA-256: `{sha256.lower() if sha256 else 'not supplied'}`\n"
        f"- Commit: `{commit.lower() if commit else 'not supplied'}`\n"
        f"- Added from [Discord thread]({thread_url}) by {reporter.replace('@', '@\u200b')}.\n\n"
        "This identifies the reported build; it does not by itself confirm the fix."
    )
