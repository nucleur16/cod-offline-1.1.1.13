'use client';

import { useRouter } from 'next/navigation';
import { useCallback, useEffect, useState } from 'react';
import { Markdown } from './Markdown';
import { request } from '@/lib/client';
import type { Page, Space } from '@/lib/types';

interface Props {
  spaces: Space[];
  /** Absent when creating a new page. */
  page?: Page;
}

const SLUG_RULE = /^[a-z0-9]+(?:[-/][a-z0-9]+)*$/;

function suggestSlug(title: string): string {
  return title
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, '-')
    .replace(/^-+|-+$/g, '');
}

/** Create or edit one page, with a live preview of the rendered markdown. */
export function PageEditor({ spaces, page }: Props) {
  const router = useRouter();
  const editing = !!page;

  const [title, setTitle] = useState(page?.title ?? '');
  const [slug, setSlug] = useState(page?.slug ?? '');
  const [slugTouched, setSlugTouched] = useState(editing);
  const [summary, setSummary] = useState(page?.summary ?? '');
  const [body, setBody] = useState(page?.body ?? '');
  const [space, setSpace] = useState(page?.space?.slug ?? spaces[0]?.slug ?? '');
  const [status, setStatus] = useState<'published' | 'draft'>(page?.status ?? 'published');
  const [tags, setTags] = useState((page?.tags ?? []).join(', '));
  const [comment, setComment] = useState('');
  const [preview, setPreview] = useState(true);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState('');

  useEffect(() => {
    if (!slugTouched) setSlug(suggestSlug(title));
  }, [title, slugTouched]);

  const save = useCallback(async () => {
    setError('');
    if (!title.trim()) return setError('A title is required.');
    if (!SLUG_RULE.test(slug))
      return setError('The address may use lowercase letters, digits, dashes and slashes only.');

    const payload = {
      slug,
      title: title.trim(),
      summary: summary.trim(),
      body,
      space_slug: space || null,
      status,
      tags: tags
        .split(',')
        .map((tag) => tag.trim())
        .filter(Boolean),
      comment: comment.trim(),
    };

    setBusy(true);
    try {
      if (editing) {
        await request<Page>(`/pages/${page!.slug}`, {
          method: 'PATCH',
          body: JSON.stringify({ ...payload, expected_version: page!.version }),
        });
      } else {
        await request<Page>('/pages', { method: 'POST', body: JSON.stringify(payload) });
      }
      router.push(`/wiki/${slug}`);
      router.refresh();
    } catch (caught) {
      setError(caught instanceof Error ? caught.message : String(caught));
    } finally {
      setBusy(false);
    }
  }, [body, comment, editing, page, router, slug, space, status, summary, tags, title]);

  useEffect(() => {
    function onKey(event: KeyboardEvent) {
      if ((event.ctrlKey || event.metaKey) && event.key === 's') {
        event.preventDefault();
        void save();
      }
    }
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [save]);

  async function remove() {
    if (!page) return;
    if (!confirm(`Delete "${page.title}" and all of its revisions? This cannot be undone.`)) return;
    setBusy(true);
    try {
      await request(`/pages/${page.slug}`, { method: 'DELETE' });
      router.push('/');
      router.refresh();
    } catch (caught) {
      setError(caught instanceof Error ? caught.message : String(caught));
      setBusy(false);
    }
  }

  return (
    <div>
      {error && (
        <div className="notice error" style={{ marginBottom: 12 }}>
          {error}
        </div>
      )}

      <div className="grid two" style={{ marginBottom: 12 }}>
        <div className="field">
          <label htmlFor="title">Title</label>
          <input
            id="title"
            value={title}
            onChange={(event) => setTitle(event.target.value)}
            placeholder="The snapshot pipeline"
          />
        </div>
        <div className="field">
          <label htmlFor="slug">Address</label>
          <input
            id="slug"
            className="mono"
            value={slug}
            onChange={(event) => {
              setSlugTouched(true);
              setSlug(event.target.value);
            }}
            placeholder="snapshot-pipeline"
          />
        </div>
      </div>

      <div className="field">
        <label htmlFor="summary">Summary</label>
        <input
          id="summary"
          value={summary}
          onChange={(event) => setSummary(event.target.value)}
          placeholder="One sentence shown in listings and search results."
        />
      </div>

      <div className="grid three" style={{ marginBottom: 12 }}>
        <div className="field">
          <label htmlFor="space">Section</label>
          <select id="space" value={space} onChange={(event) => setSpace(event.target.value)}>
            <option value="">Unfiled</option>
            {spaces.map((option) => (
              <option key={option.slug} value={option.slug}>
                {option.title}
              </option>
            ))}
          </select>
        </div>
        <div className="field">
          <label htmlFor="status">Status</label>
          <select
            id="status"
            value={status}
            onChange={(event) => setStatus(event.target.value as 'published' | 'draft')}
          >
            <option value="published">Published</option>
            <option value="draft">Draft — editors only</option>
          </select>
        </div>
        <div className="field">
          <label htmlFor="tags">Tags</label>
          <input
            id="tags"
            value={tags}
            onChange={(event) => setTags(event.target.value)}
            placeholder="snapshot, net"
          />
        </div>
      </div>

      <div className="toolbar">
        <button className="button primary" onClick={save} disabled={busy}>
          {busy ? 'Saving…' : editing ? 'Save changes' : 'Create page'}
        </button>
        <button className="button" onClick={() => setPreview((value) => !value)}>
          {preview ? 'Hide preview' : 'Show preview'}
        </button>
        {editing && (
          <input
            style={{ maxWidth: 320 }}
            value={comment}
            onChange={(event) => setComment(event.target.value)}
            placeholder="What changed? (kept with the revision)"
          />
        )}
        <div className="spacer" />
        {editing && (
          <button className="button danger" onClick={remove} disabled={busy}>
            Delete
          </button>
        )}
      </div>

      <div className={preview ? 'editor-split' : ''}>
        <textarea
          value={body}
          onChange={(event) => setBody(event.target.value)}
          spellCheck={false}
          placeholder={'# Heading\n\nMarkdown, with GitHub tables and fenced code blocks.'}
          aria-label="Page body"
        />
        {preview && (
          <div className="editor-preview">
            <Markdown>{body || '_Nothing to preview yet._'}</Markdown>
          </div>
        )}
      </div>

      <p className="faint small" style={{ marginTop: 10 }}>
        Ctrl+S saves. Every save keeps the previous text as a revision you can read or restore from
        the page history.
      </p>
    </div>
  );
}
