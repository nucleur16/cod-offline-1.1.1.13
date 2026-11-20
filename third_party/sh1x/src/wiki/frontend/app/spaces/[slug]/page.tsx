import Link from 'next/link';
import { notFound } from 'next/navigation';
import { api, query } from '@/lib/api';
import type { Paged, PageSummary, Space } from '@/lib/types';

export const dynamic = 'force-dynamic';

export default async function SpacePage({ params }: { params: Promise<{ slug: string }> }) {
  const slug = (await params).slug;
  const [spaces, pages] = await Promise.all([
    api<Space[]>('/spaces'),
    api<Paged<PageSummary>>('/pages' + query({ space: slug, limit: 300 })),
  ]);
  const space = spaces.find((candidate) => candidate.slug === slug);
  if (!space) notFound();

  return (
    <main className="content narrow">
      <div className="page-head">
        <div className="crumbs">
          <Link href="/">wiki</Link> / {space.slug}
        </div>
        <h1>{space.title}</h1>
        {space.summary && <p className="lede">{space.summary}</p>}
      </div>

      {pages.items.length === 0 ? (
        <div className="empty">No pages here yet.</div>
      ) : (
        <div className="list">
          {pages.items.map((page) => (
            <Link className="list-item" key={page.slug} href={`/wiki/${page.slug}`}>
              <div className="row">
                <strong>{page.title}</strong>
                {page.status === 'draft' && <span className="badge warn">draft</span>}
                <div className="spacer" />
                <span className="faint small">
                  {page.updated_at ? new Date(page.updated_at).toLocaleDateString() : ''}
                </span>
              </div>
              {page.summary && (
                <p className="muted small" style={{ margin: '4px 0 0' }}>
                  {page.summary}
                </p>
              )}
            </Link>
          ))}
        </div>
      )}
    </main>
  );
}
