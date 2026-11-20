'use client';

import { useRouter } from 'next/navigation';
import { useState } from 'react';
import { request, signOut } from '@/lib/client';

export function AccountPanel() {
  const router = useRouter();
  const [current, setCurrent] = useState('');
  const [next, setNext] = useState('');
  const [message, setMessage] = useState('');
  const [error, setError] = useState('');
  const [busy, setBusy] = useState(false);

  async function change(event: React.FormEvent) {
    event.preventDefault();
    setError('');
    setMessage('');
    if (next.length < 8) return setError('Choose a password of at least 8 characters.');
    setBusy(true);
    try {
      await request('/auth/password', {
        method: 'POST',
        body: JSON.stringify({ current_password: current, new_password: next }),
      });
      setMessage('Password changed.');
      setCurrent('');
      setNext('');
    } catch (caught) {
      setError(caught instanceof Error ? caught.message : String(caught));
    } finally {
      setBusy(false);
    }
  }

  async function out() {
    await signOut();
    router.push('/');
    router.refresh();
  }

  return (
    <div className="stack">
      <form className="card" onSubmit={change}>
        <strong>Change password</strong>
        {error && (
          <div className="notice error" style={{ margin: '10px 0' }}>
            {error}
          </div>
        )}
        {message && (
          <div className="notice ok" style={{ margin: '10px 0' }}>
            {message}
          </div>
        )}
        <div className="field" style={{ marginTop: 10 }}>
          <label htmlFor="current">Current password</label>
          <input
            id="current"
            type="password"
            value={current}
            autoComplete="current-password"
            onChange={(event) => setCurrent(event.target.value)}
          />
        </div>
        <div className="field">
          <label htmlFor="next">New password</label>
          <input
            id="next"
            type="password"
            value={next}
            autoComplete="new-password"
            onChange={(event) => setNext(event.target.value)}
          />
        </div>
        <button className="button primary" type="submit" disabled={busy}>
          Change password
        </button>
      </form>

      <button className="button" onClick={out}>
        Sign out
      </button>
    </div>
  );
}
