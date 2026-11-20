import Link from 'next/link';
import { redirect } from 'next/navigation';
import { currentUser, isAdmin } from '@/lib/session';
import { AccountPanel } from '@/components/AccountPanel';

export const dynamic = 'force-dynamic';
export const metadata = { title: 'Account' };

export default async function Account() {
  const user = await currentUser();
  if (!user) redirect('/login?next=/account');

  return (
    <main className="content narrow" style={{ maxWidth: 560 }}>
      <div className="page-head">
        <h1>{user.display_name || user.username}</h1>
        <p className="lede">
          Signed in as <code>{user.username}</code> with the <strong>{user.role}</strong> role.
          {user.role === 'reader' && ' Readers can browse the wiki but not change it.'}
        </p>
      </div>
      <AccountPanel />
      {isAdmin(user) && (
        <p style={{ marginTop: 18 }}>
          <Link href="/admin">Administration →</Link>
        </p>
      )}
    </main>
  );
}
