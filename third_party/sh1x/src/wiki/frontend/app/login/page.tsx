import { redirect } from 'next/navigation';
import { currentUser } from '@/lib/session';
import { LoginForm } from '@/components/LoginForm';

export const dynamic = 'force-dynamic';
export const metadata = { title: 'Sign in' };

type Search = Promise<{ next?: string }>;

export default async function Login({ searchParams }: { searchParams: Search }) {
  const user = await currentUser();
  const next = (await searchParams).next ?? '/';
  if (user) redirect(next);

  return (
    <main className="content narrow" style={{ maxWidth: 440 }}>
      <div className="page-head">
        <h1>Sign in</h1>
        <p className="lede">
          Editors and administrators can change pages. Everyone else can read the wiki without an
          account.
        </p>
      </div>
      <LoginForm next={next} />
      <p className="faint small" style={{ marginTop: 18 }}>
        The first administrator is created from the command line:
        <br />
        <code>python -m app.cli admin --username you</code>
      </p>
    </main>
  );
}
