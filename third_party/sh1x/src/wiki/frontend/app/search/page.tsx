import Link from 'next/link';
import { api, query } from '@/lib/api';
import type { SearchResponse } from '@/lib/types';

export const dynamic = 'force-dynamic';
export const metadata = { title: 'Search' };

type Search = Promise<Record<string, string | string[] | undefined>>;

const FILTERS = [
  ['all', 'Everything'],
  ['pages', 'Wiki pages'],
  ['symbols', 'Declarations'],
  ['builtins', 'GSC builtins'],
];

export default async function SearchPage({ searchParams }: { searchParams: Search }) {
  const params = await searchParams;
  const q = (Array.isArray(params.q) ? params.q[0] : params.q) ?? '';
  const kind = (Array.isArray(params.kind) ? params.kind[0] : params.kind) ?? 'all';

  if (!q.trim()) {
    return (
      <main className="content narrow">
        <div className="page-head">
          <h1>Search</h1>
          <p className="lede">
            Type in the box above. Quoted phrases and <code>-excluded</code> words work on prose;
            identifiers also match by prefix, so <code>CODOL_Write</code> finds every writer.
          </p>
        </div>
      </main>
    );
  }

  const results = await api<SearchResponse>('/search' + query({ q, kind, limit: 60 }));

  return (
    <main className="content narrow">
      <div className="page-head">
        <h1>“{q}”</h1>
        <p className="lede">
          {results.hits.length} result{results.hits.length === 1 ? '' : 's'}
          {Object.entries(results.counts).length > 0 && (
            <span className="faint">
              {' '}
              ·{' '}
              {Object.entries(results.counts)
                .map(([key, value]) => `${value} ${key}`)
                .join(', ')}
            </span>
          )}
        </p>
      </div>

      <div className="chip-row" style={{ marginBottom: 16 }}>
        {FILTERS.map(([value, label]) => (
          <Link
            key={value}
            className={kind === value ? 'badge accent' : 'badge'}
            href={`/search${query({ q, kind: value })}`}
          >
            {label}
          </Link>
        ))}
      </div>

      {results.hits.length === 0 ? (
        <div className="empty">
          Nothing matched. Try a shorter identifier prefix, or search the declarations directly.
        </div>
      ) : (
        <div className="list">
          {results.hits.map((hit, index) => (
            <Link className="list-item" key={`${hit.href}-${index}`} href={hit.href}>
              <div className="row">
                <strong className={hit.type === 'page' ? '' : 'mono'}>{hit.title}</strong>
                <span className="badge">{hit.type}</span>
                <div className="spacer" />
                <span className="faint small">{hit.subtitle}</span>
              </div>
              {hit.snippet && (
                <p className="muted small" style={{ margin: '4px 0 0' }}>
                  {hit.snippet}
                </p>
              )}
            </Link>
          ))}
        </div>
      )}
    </main>
  );
}
