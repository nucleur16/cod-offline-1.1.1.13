import Link from 'next/link';
import { notFound, redirect } from 'next/navigation';
import { api, apiOrNull } from '@/lib/api';
import { canEdit, currentUser } from '@/lib/session';
import { PageEditor } from '@/components/PageEditor';
import type { Page, Space } from '@/lib/types';

export const dynamic = 'force-dynamic';
export const metadata = { title: 'Edit' };

export default async function EditPage({ params }: { params: Promise<{ slug: string[] }> }) {
  const slug = (await params).slug.join('/');
  const user = await currentUser();
  if (!canEdit(user)) redirect(`/login?next=${encodeURIComponent(`/edit/${slug}`)}`);

  const [page, spaces] = await Promise.all([
    apiOrNull<Page>(`/pages/${slug}`),
    api<Space[]>('/spaces'),
  ]);
  if (!page) notFound();

  return (
    <main className="content">
      <div className="page-head">
        <div className="crumbs">
          <Link href={`/wiki/${slug}`}>{slug}</Link> / edit
        </div>
        <h1>Editing “{page.title}”</h1>
        <p className="lede">
          Saving files the current text as version {page.version} and publishes yours as version{' '}
          {page.version + 1}.
        </p>
      </div>
      <PageEditor spaces={spaces} page={page} />
    </main>
  );
}
