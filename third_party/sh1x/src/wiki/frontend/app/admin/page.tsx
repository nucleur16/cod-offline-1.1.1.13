import { redirect } from 'next/navigation';
import { api } from '@/lib/api';
import { currentUser, isAdmin } from '@/lib/session';
import { UserAdmin } from '@/components/UserAdmin';
import { SpaceAdmin } from '@/components/SpaceAdmin';
import type { IndexRun, Space, User } from '@/lib/types';

export const dynamic = 'force-dynamic';
export const metadata = { title: 'Administration' };

export default async function Admin() {
  const me = await currentUser();
  if (!isAdmin(me)) redirect('/login?next=/admin');

  const [users, spaces, runs] = await Promise.all([
    api<User[]>('/auth/users'),
    api<Space[]>('/spaces'),
    api<IndexRun[]>('/reference/runs'),
  ]);

  return (
    <main className="content">
      <div className="page-head">
        <h1>Administration</h1>
        <p className="lede">
          Accounts, sections, and the state of the generated reference.
        </p>
      </div>

      <section style={{ marginBottom: 30 }}>
        <h2 style={{ fontSize: 16 }}>Accounts</h2>
        <p className="faint small">
          <strong>admin</strong> manages accounts and sections; <strong>editor</strong> may write
          pages; <strong>reader</strong> is read-only — the same as being signed out, but with
          drafts hidden either way.
        </p>
        <UserAdmin users={users} me={me!.username} />
      </section>

      <section style={{ marginBottom: 30 }}>
        <h2 style={{ fontSize: 16 }}>Sections</h2>
        <p className="faint small">
          Sections group pages in the sidebar and on the front page. A section has to be empty before
          it can be deleted.
        </p>
        <SpaceAdmin spaces={spaces} />
      </section>

      <section>
        <h2 style={{ fontSize: 16 }}>Reference index</h2>
        <p className="faint small">
          Rebuilt from the command line, not from the browser: the indexer reads the source tree, so
          it runs where that tree is. In <code>wiki/backend</code>, run{' '}
          <code>python -m app.cli index</code>.
        </p>
        <div className="table-wrap">
          <table className="table">
            <thead>
              <tr>
                <th>Run</th>
                <th>Finished</th>
                <th>Commit</th>
                <th>Files</th>
                <th>Declarations</th>
                <th>Builtins</th>
              </tr>
            </thead>
            <tbody>
              {runs.map((run) => (
                <tr key={run.id}>
                  <td className="mono">#{run.id}</td>
                  <td className="small">
                    {run.finished_at ? new Date(run.finished_at).toLocaleString() : 'incomplete'}
                  </td>
                  <td className="mono faint">{run.git_commit.slice(0, 10) || '—'}</td>
                  <td className="mono">{run.files}</td>
                  <td className="mono">{run.symbols.toLocaleString('en-US')}</td>
                  <td className="mono">{run.builtins}</td>
                </tr>
              ))}
              {!runs.length && (
                <tr>
                  <td colSpan={6} className="faint">
                    The index has never been built.
                  </td>
                </tr>
              )}
            </tbody>
          </table>
        </div>
      </section>
    </main>
  );
}
