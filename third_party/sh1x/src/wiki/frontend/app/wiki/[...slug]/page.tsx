import Link from 'next/link';
import { notFound } from 'next/navigation';
import type { Metadata } from 'next';
import { apiOrNull } from '@/lib/api';
import { canEdit, currentUser } from '@/lib/session';
import { Markdown, headings } from '@/components/Markdown';
import type { Page } from '@/lib/types';

export const dynamic = 'force-dynamic';

type Props = { params: Promise<{ slug: string[] }> };

async function load(slug: string[]): Promise<Page | null> {
  return apiOrNull<Page>(`/pages/${slug.map(encodeURIComponent).join('/')}`);
}

export async function generateMetadata({ params }: Props): Promise<Metadata> {
  const page = await load((await params).slug);
  return page ? { title: page.title, description: page.summary } : { title: 'Not found' };
}

export default async function WikiPage({ params }: Props) {
  const slug = (await params).slug;
  const [page, user] = await Promise.all([load(slug), currentUser()]);
  if (!page) notFound();

  const path = slug.join('/');
  const toc = headings(page.body);

  return (
    <main className="content">
      <div className="page-head">
        <div className="crumbs">
          <Link href="/">wiki</Link>
          {page.space && (
            <>
              {' / '}
              <Link href={`/spaces/${page.space.slug}`}>{page.space.title}</Link>
            </>
          )}
          {' / '}
          {path}
        </div>
        <h1>{page.title}</h1>
        {page.summary && <p className="lede">{page.summary}</p>}
        <div className="row small faint" style={{ marginTop: 10 }}>
          <span>
            version {page.version} · updated{' '}
            {page.updated_at ? new Date(page.updated_at).toLocaleString() : 'unknown'}
            {page.updated_by && ` by ${page.updated_by.display_name || page.updated_by.username}`}
          </span>
          {page.status === 'draft' && <span className="badge warn">draft</span>}
          {page.origin === 'seed' && (
            <span className="badge" title="Shipped with the repository as wiki/content markdown.">
              seeded
            </span>
          )}
          {page.tags?.map((tag) => (
            <span className="badge" key={tag}>
              {tag}
            </span>
          ))}
          <div className="spacer" />
          <Link href={`/history/${path}`}>History</Link>
          {canEdit(user) && <Link href={`/edit/${path}`}>Edit</Link>}
        </div>
      </div>

      <div className="with-toc">
        <article>
          <Markdown>{page.body}</Markdown>
        </article>
        {toc.length > 2 && (
          <aside className="toc">
            <p className="nav-heading">On this page</p>
            {toc.map((heading) => (
              <a
                key={heading.id}
                href={`#${heading.id}`}
                style={{ paddingLeft: heading.depth === 3 ? 12 : 0 }}
              >
                {heading.text}
              </a>
            ))}
          </aside>
        )}
      </div>
    </main>
  );
}
