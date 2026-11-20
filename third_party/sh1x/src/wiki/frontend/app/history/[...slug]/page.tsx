import Link from 'next/link';
import { notFound } from 'next/navigation';
import { apiOrNull } from '@/lib/api';
import { canEdit, currentUser } from '@/lib/session';
import { RevertButton } from '@/components/RevertButton';
import { Markdown } from '@/components/Markdown';
import type { Page, Revision, RevisionDetail } from '@/lib/types';

export const dynamic = 'force-dynamic';
export const metadata = { title: 'History' };

type Props = {
  params: Promise<{ slug: string[] }>;
  searchParams: Promise<{ v?: string }>;
};

export default async function History({ params, searchParams }: Props) {
  const slug = (await params).slug.join('/');
  const version = (await searchParams).v;

  const [page, revisions, user] = await Promise.all([
    apiOrNull<Page>(`/pages/${slug}`),
    apiOrNull<Revision[]>(`/pages/${slug}/revisions`),
    currentUser(),
  ]);
  if (!page) notFound();

  // `?v=N` reads one old version in full rather than listing them.
  if (version) {
    const revision = await apiOrNull<RevisionDetail>(`/pages/${slug}/revisions/${version}`);
    if (!revision) notFound();
    return (
      <main className="content narrow">
        <div className="page-head">
          <div className="crumbs">
            <Link href={`/wiki/${slug}`}>{slug}</Link> /{' '}
            <Link href={`/history/${slug}`}>history</Link> / v{revision.version}
          </div>
          <h1>{revision.title}</h1>
          <p className="lede">
            Version {revision.version}, saved{' '}
            {revision.created_at
              ? new Date(revision.created_at).toLocaleString()
              : 'at an unknown time'}
            {revision.author && ` by ${revision.author.display_name || revision.author.username}`}.
          </p>
          {revision.comment && <p className="muted small">{revision.comment}</p>}
        </div>
        <div className="notice">
          This is an old version. <Link href={`/wiki/${slug}`}>Read the current one</Link>, or{' '}
          <Link href={`/history/${slug}`}>go back to the history</Link>.
        </div>
        <div style={{ marginTop: 16 }}>
          <Markdown>{revision.body}</Markdown>
        </div>
      </main>
    );
  }

  return (
    <main className="content narrow">
      <div className="page-head">
        <div className="crumbs">
          <Link href={`/wiki/${slug}`}>{slug}</Link> / history
        </div>
        <h1>History of “{page.title}”</h1>
        <p className="lede">
          Version {page.version} is live. Each earlier entry is the full text as it stood before the
          save that replaced it.
        </p>
      </div>

      <div className="list">
        <div className="list-item">
          <div className="row">
            <Link href={`/wiki/${slug}`}>
              <strong>Version {page.version}</strong>
            </Link>
            <span className="badge accent">current</span>
            <div className="spacer" />
            <span className="faint small">
              {page.updated_at ? new Date(page.updated_at).toLocaleString() : ''}
              {page.updated_by && ` · ${page.updated_by.display_name || page.updated_by.username}`}
            </span>
          </div>
        </div>

        {(revisions ?? []).map((revision) => (
          <div className="list-item" key={revision.id}>
            <div className="row">
              <Link href={`/history/${slug}?v=${revision.version}`}>
                <strong>Version {revision.version}</strong>
              </Link>
              <span className="muted small">{revision.title}</span>
              <div className="spacer" />
              <span className="faint small">
                {revision.created_at ? new Date(revision.created_at).toLocaleString() : ''}
                {revision.author && ` · ${revision.author.display_name || revision.author.username}`}
              </span>
              {canEdit(user) && <RevertButton slug={slug} version={revision.version} />}
            </div>
            {revision.comment && (
              <p className="muted small" style={{ margin: '4px 0 0' }}>
                {revision.comment}
              </p>
            )}
          </div>
        ))}

        {!revisions?.length && (
          <div className="list-item faint">This page has not been edited since it was created.</div>
        )}
      </div>
    </main>
  );
}
