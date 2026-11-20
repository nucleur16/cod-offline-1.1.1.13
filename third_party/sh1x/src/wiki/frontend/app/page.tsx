import Link from 'next/link';
import { api, query } from '@/lib/api';
import type { Area, Paged, PageSummary, SiteStats, Space } from '@/lib/types';

export const dynamic = 'force-dynamic';

function number(value: number): string {
  return value.toLocaleString('en-US');
}

export default async function Home() {
  let stats: SiteStats | null = null;
  let spaces: Space[] = [];
  let pages: PageSummary[] = [];
  let areas: Area[] = [];
  let error = '';

  try {
    [stats, spaces, pages, areas] = await Promise.all([
      api<SiteStats>('/site'),
      api<Space[]>('/spaces'),
      api<Paged<PageSummary>>('/pages' + query({ limit: 500 })).then((p) => p.items),
      api<Area[]>('/reference/areas'),
    ]);
  } catch (caught) {
    error = caught instanceof Error ? caught.message : String(caught);
  }

  if (error) {
    return (
      <main className="content narrow">
        <div className="page-head">
          <h1>sh1x server wiki</h1>
        </div>
        <div className="notice error">
          The wiki API did not answer: {error}
          <br />
          Start it with <code>uvicorn app.main:app</code> in <code>wiki/backend</code>, or
          <code> docker compose up</code> in <code>wiki/</code>.
        </div>
      </main>
    );
  }

  const asm = stats?.implementations['x86-asm'] ?? 0;
  const codol = areas.filter((area) => area.is_codol);

  return (
    <main className="content">
      <div className="page-head">
        <h1>{stats?.site_name ?? 'sh1x server wiki'}</h1>
        <p className="lede">
          {stats?.tagline}. This wiki documents the server as it stands in this working tree: the
          hand-written notes below, plus a reference index built by reading every source file in{' '}
          <code>src/</code>.
        </p>
      </div>

      <div className="grid three" style={{ marginBottom: 26 }}>
        <div className="stat">
          <div className="value">{number(stats?.symbols ?? 0)}</div>
          <div className="label">declarations indexed</div>
        </div>
        <div className="stat">
          <div className="value">{number(stats?.builtins ?? 0)}</div>
          <div className="label">GSC builtins registered</div>
        </div>
        <div className="stat">
          <div className="value">{number(asm)}</div>
          <div className="label">functions still in x86 asm</div>
        </div>
        <div className="stat">
          <div className="value">{number(stats?.files ?? 0)}</div>
          <div className="label">source files</div>
        </div>
        <div className="stat">
          <div className="value">{number(stats?.lines ?? 0)}</div>
          <div className="label">lines of source</div>
        </div>
        <div className="stat">
          <div className="value">{number(stats?.pages ?? 0)}</div>
          <div className="label">wiki pages</div>
        </div>
      </div>

      {spaces.map((space) => {
        const inSpace = pages.filter((page) => page.space?.slug === space.slug);
        if (!inSpace.length) return null;
        return (
          <section key={space.slug} style={{ marginBottom: 26 }}>
            <h2 style={{ fontSize: 17, margin: '0 0 4px' }}>{space.title}</h2>
            <p className="muted small" style={{ margin: '0 0 10px' }}>
              {space.summary}
            </p>
            <div className="grid two">
              {inSpace.map((page) => (
                <Link className="card" key={page.slug} href={`/wiki/${page.slug}`}>
                  <strong>{page.title}</strong>
                  {page.status === 'draft' && <span className="badge warn"> draft</span>}
                  <p className="muted small" style={{ margin: '4px 0 0' }}>
                    {page.summary}
                  </p>
                </Link>
              ))}
            </div>
          </section>
        );
      })}

      <section>
        <h2 style={{ fontSize: 17, margin: '0 0 4px' }}>Source areas</h2>
        <p className="muted small" style={{ margin: '0 0 10px' }}>
          The CoDOL work lives under <code>src/codol/&lt;area&gt;/</code>. Counts come from the last
          index run.
        </p>
        <div className="table-wrap">
          <table className="table">
            <thead>
              <tr>
                <th>Area</th>
                <th className="mono">path</th>
                <th>Files</th>
                <th>Lines</th>
                <th>Declarations</th>
              </tr>
            </thead>
            <tbody>
              {codol.map((area) => (
                <tr key={area.name}>
                  <td>
                    <Link href={`/reference/areas/${area.name}`}>{area.title}</Link>
                  </td>
                  <td className="mono faint">{area.path}</td>
                  <td className="mono">{area.file_count}</td>
                  <td className="mono">{number(area.line_count)}</td>
                  <td className="mono">{number(area.symbol_count)}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </section>

      {stats?.last_index && (
        <p className="faint small" style={{ marginTop: 18 }}>
          Reference last indexed{' '}
          {stats.last_index.finished_at
            ? new Date(stats.last_index.finished_at).toLocaleString()
            : 'unknown'}
          {stats.last_index.git_commit && (
            <>
              {' '}
              at commit <code>{stats.last_index.git_commit.slice(0, 10)}</code>
            </>
          )}
          .
        </p>
      )}
    </main>
  );
}
