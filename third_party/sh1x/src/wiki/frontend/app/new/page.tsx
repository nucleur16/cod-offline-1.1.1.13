import { redirect } from 'next/navigation';
import { api } from '@/lib/api';
import { canEdit, currentUser } from '@/lib/session';
import { PageEditor } from '@/components/PageEditor';
import type { Space } from '@/lib/types';

export const dynamic = 'force-dynamic';
export const metadata = { title: 'New page' };

export default async function NewPage() {
  const user = await currentUser();
  if (!canEdit(user)) redirect('/login?next=/new');
  const spaces = await api<Space[]>('/spaces');

  return (
    <main className="content">
      <div className="page-head">
        <h1>New page</h1>
        <p className="lede">
          Written in markdown. Pick the section it belongs to so it appears in the sidebar and on the
          front page.
        </p>
      </div>
      <PageEditor spaces={spaces} />
    </main>
  );
}
