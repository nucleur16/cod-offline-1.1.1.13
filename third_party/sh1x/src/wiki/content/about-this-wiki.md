---
slug: about-this-wiki
title: About this wiki
space: wiki
summary: How the wiki is built and run, how to edit a page, and how to add accounts.
position: 10
tags: [wiki]
---

The wiki is a small application in `wiki/`, separate from the server it
documents. It never builds, runs or writes to the server tree; it only reads it.

## The parts

| Part | Stack | Lives in |
|---|---|---|
| Frontend | Next.js 15 (App Router), TypeScript, hand-written CSS | `wiki/frontend/` |
| Backend | FastAPI, SQLAlchemy 2 async, asyncpg | `wiki/backend/` |
| Database | PostgreSQL 16 | `docker compose` service `db` |
| Indexer | Python, lexical C/C++/asm scanner | `wiki/backend/app/indexer/` |
| Seed content | Markdown with frontmatter | `wiki/content/` |

Two kinds of content live side by side. **Authored pages** are written by people
and editable in the browser, with every save keeping a revision. The **generated
reference** is rebuilt from the source by the indexer and is read-only in the
UI, because the source is the truth — see [Reading the reference
index](/wiki/reading-the-reference).

## Running it

With Docker, from `wiki/`:

```sh
cp .env.example .env      # then edit WIKI_SECRET_KEY
docker compose up -d
docker compose exec backend python -m app.cli bootstrap
```

Without Docker, with Postgres already running:

```sh
cd wiki/backend
python -m venv .venv && .venv/bin/pip install -r requirements.txt
python -m app.cli bootstrap          # schema, admin, index, seed content
uvicorn app.main:app --port 8000

cd ../frontend
npm install
WIKI_API_URL=http://127.0.0.1:8000 npm run dev
```

`bootstrap` is `init` + `admin` + `index` + `seed` in order. The admin password
comes from `WIKI_ADMIN_PASSWORD`, or a prompt, or is generated and printed.

## Editing

Sign in, then **Edit** on any page. The editor is markdown with a live preview;
Ctrl+S saves. Saving files the current text as a revision and publishes yours as
the next version, so nothing is ever overwritten without a way back —
**History** on any page lists every version, shows it in full, and can restore
it.

Concurrent edits are caught rather than merged: the editor sends the version it
loaded, and a save against a stale version is refused with a message telling you
to reload.

Set a page to **draft** to keep it out of the sidebar, listings and search for
everyone who cannot edit.

**New page** creates one. Pick the section it belongs to so it appears in the
sidebar and on the front page, and give it tags — a tag that matches a source
area name (`net`, `snapshot`, `player`, …) makes the page show up on that
[area's reference page](/reference).

## Roles

| Role | Can |
|---|---|
| `admin` | Everything, plus accounts and sections. |
| `editor` | Create, edit and delete pages; see drafts. |
| `reader` | Read published pages. Same as being signed out. |

The first administrator is created from the command line; after that, accounts
are managed at **/admin**.

```sh
python -m app.cli admin --username you
```

## Keeping the reference current

```sh
cd wiki/backend
python -m app.cli index
```

Re-run it after any source change you want reflected. It replaces the reference
tables wholesale and never touches authored pages, revisions or accounts. Each
run records the git commit it read, and the front page shows when the last one
finished.

## Seeding

`wiki/content/*.md` are the pages that ship with the repository. Each starts
with a small frontmatter block:

```markdown
---
slug: subsystem-net
title: Transport
space: subsystems
summary: One sentence for listings and search.
position: 10
tags: [net]
---
```

`python -m app.cli seed` loads them. It is idempotent and **will not overwrite a
seed page that has since been edited in the browser** unless you pass `--force`,
which files the current text as a revision first.

So the two ways of writing a page compose: edit in the browser for speed, and
move the text back into `wiki/content/` when you want it in the repository.

## Security notes

- The session token is a JWT in an **httpOnly** cookie set by a Next route
  handler. Page scripts cannot read it, so a stored XSS in rendered markdown
  cannot lift an editor's session.
- Browser mutations go through `/api/proxy/...`, which attaches the cookie
  server-side. The backend does not need to be reachable from the browser at
  all.
- Passwords are bcrypt hashes. Sign-in verifies a hash even when the username
  does not exist, so a wrong username and a wrong password take the same time.
- `WIKI_SECRET_KEY` signs the tokens. Change it before putting this anywhere
  other than a laptop.
- The repository is mounted **read-only** into the backend container.
