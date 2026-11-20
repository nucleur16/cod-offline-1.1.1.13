'use client';

import { useRouter } from 'next/navigation';
import { useState } from 'react';
import { request } from '@/lib/client';
import type { Space } from '@/lib/types';

export function SpaceAdmin({ spaces }: { spaces: Space[] }) {
  const router = useRouter();
  const [error, setError] = useState('');
  const [busy, setBusy] = useState(false);
  const [form, setForm] = useState({ slug: '', title: '', summary: '', position: 100 });

  async function act(work: () => Promise<unknown>) {
    setError('');
    setBusy(true);
    try {
      await work();
      router.refresh();
    } catch (caught) {
      setError(caught instanceof Error ? caught.message : String(caught));
    } finally {
      setBusy(false);
    }
  }

  return (
    <div className="stack">
      {error && <div className="notice error">{error}</div>}

      <div className="table-wrap">
        <table className="table">
          <thead>
            <tr>
              <th>Slug</th>
              <th>Title</th>
              <th>Summary</th>
              <th>Order</th>
              <th />
            </tr>
          </thead>
          <tbody>
            {spaces.map((space) => (
              <tr key={space.id}>
                <td className="mono">{space.slug}</td>
                <td>{space.title}</td>
                <td className="muted small">{space.summary}</td>
                <td className="mono">{space.position}</td>
                <td>
                  <button
                    className="button danger"
                    disabled={busy}
                    onClick={() => {
                      if (!confirm(`Delete the section "${space.title}"?`)) return;
                      act(() => request(`/spaces/${space.slug}`, { method: 'DELETE' }));
                    }}
                  >
                    Delete
                  </button>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>

      <form
        className="card"
        onSubmit={(event) => {
          event.preventDefault();
          act(async () => {
            await request('/spaces', {
              method: 'POST',
              body: JSON.stringify({ ...form, source_area: '' }),
            });
            setForm({ slug: '', title: '', summary: '', position: 100 });
          });
        }}
      >
        <strong>Add a section</strong>
        <div className="grid three" style={{ marginTop: 10 }}>
          <div className="field">
            <label htmlFor="space-slug">Slug</label>
            <input
              id="space-slug"
              className="mono"
              value={form.slug}
              placeholder="protocols"
              onChange={(event) => setForm({ ...form, slug: event.target.value })}
            />
          </div>
          <div className="field">
            <label htmlFor="space-title">Title</label>
            <input
              id="space-title"
              value={form.title}
              onChange={(event) => setForm({ ...form, title: event.target.value })}
            />
          </div>
          <div className="field">
            <label htmlFor="space-position">Order</label>
            <input
              id="space-position"
              type="number"
              value={form.position}
              onChange={(event) => setForm({ ...form, position: Number(event.target.value) })}
            />
          </div>
        </div>
        <div className="field">
          <label htmlFor="space-summary">Summary</label>
          <input
            id="space-summary"
            value={form.summary}
            onChange={(event) => setForm({ ...form, summary: event.target.value })}
          />
        </div>
        <button className="button primary" type="submit" disabled={busy}>
          Create section
        </button>
      </form>
    </div>
  );
}
