'use client';

import { useRouter } from 'next/navigation';
import { useState } from 'react';
import { request } from '@/lib/client';
import type { Role, User } from '@/lib/types';

const ROLES: Role[] = ['admin', 'editor', 'reader'];

export function UserAdmin({ users, me }: { users: User[]; me: string }) {
  const router = useRouter();
  const [error, setError] = useState('');
  const [busy, setBusy] = useState(false);
  const [form, setForm] = useState({
    username: '',
    password: '',
    display_name: '',
    email: '',
    role: 'editor' as Role,
  });

  async function act<T>(work: () => Promise<T>) {
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
              <th>Username</th>
              <th>Name</th>
              <th>Role</th>
              <th>Active</th>
              <th>Last signed in</th>
              <th />
            </tr>
          </thead>
          <tbody>
            {users.map((user) => (
              <tr key={user.id}>
                <td className="mono">
                  {user.username}
                  {user.username === me && <span className="badge accent"> you</span>}
                </td>
                <td>{user.display_name}</td>
                <td>
                  <select
                    value={user.role}
                    disabled={busy || user.username === me}
                    onChange={(event) =>
                      act(() =>
                        request(`/auth/users/${user.username}`, {
                          method: 'PATCH',
                          body: JSON.stringify({ role: event.target.value }),
                        }),
                      )
                    }
                  >
                    {ROLES.map((role) => (
                      <option key={role} value={role}>
                        {role}
                      </option>
                    ))}
                  </select>
                </td>
                <td>
                  <input
                    type="checkbox"
                    style={{ width: 'auto' }}
                    checked={user.is_active}
                    disabled={busy || user.username === me}
                    onChange={(event) =>
                      act(() =>
                        request(`/auth/users/${user.username}`, {
                          method: 'PATCH',
                          body: JSON.stringify({ is_active: event.target.checked }),
                        }),
                      )
                    }
                  />
                </td>
                <td className="faint small">
                  {user.last_login_at ? new Date(user.last_login_at).toLocaleString() : 'never'}
                </td>
                <td>
                  {user.username !== me && (
                    <button
                      className="button danger"
                      disabled={busy}
                      onClick={() => {
                        if (!confirm(`Delete the account "${user.username}"?`)) return;
                        act(() => request(`/auth/users/${user.username}`, { method: 'DELETE' }));
                      }}
                    >
                      Delete
                    </button>
                  )}
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
            await request('/auth/users', { method: 'POST', body: JSON.stringify(form) });
            setForm({ username: '', password: '', display_name: '', email: '', role: 'editor' });
          });
        }}
      >
        <strong>Add an account</strong>
        <div className="grid three" style={{ marginTop: 10 }}>
          <div className="field">
            <label htmlFor="new-username">Username</label>
            <input
              id="new-username"
              value={form.username}
              onChange={(event) => setForm({ ...form, username: event.target.value })}
            />
          </div>
          <div className="field">
            <label htmlFor="new-display">Display name</label>
            <input
              id="new-display"
              value={form.display_name}
              onChange={(event) => setForm({ ...form, display_name: event.target.value })}
            />
          </div>
          <div className="field">
            <label htmlFor="new-role">Role</label>
            <select
              id="new-role"
              value={form.role}
              onChange={(event) => setForm({ ...form, role: event.target.value as Role })}
            >
              {ROLES.map((role) => (
                <option key={role} value={role}>
                  {role}
                </option>
              ))}
            </select>
          </div>
          <div className="field">
            <label htmlFor="new-email">Email (optional)</label>
            <input
              id="new-email"
              value={form.email}
              onChange={(event) => setForm({ ...form, email: event.target.value })}
            />
          </div>
          <div className="field">
            <label htmlFor="new-password">Password (8+ characters)</label>
            <input
              id="new-password"
              type="password"
              value={form.password}
              autoComplete="new-password"
              onChange={(event) => setForm({ ...form, password: event.target.value })}
            />
          </div>
        </div>
        <button className="button primary" type="submit" disabled={busy}>
          Create account
        </button>
      </form>
    </div>
  );
}
