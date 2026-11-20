import Link from 'next/link';
import { api, query } from '@/lib/api';
import { ImplementationBadge } from '@/components/badges';
import { SymbolFilters } from '@/components/SymbolFilters';
import type { Area, Paged, SymbolSummary } from '@/lib/types';

export const dynamic = 'force-dynamic';
export const metadata = { title: 'Declarations' };

type Search = Promise<Record<string, string | string[] | undefined>>;

function one(value: string | string[] | undefined): string {
  return Array.isArray(value) ? (value[0] ?? '') : (value ?? '');
}

export default async function Symbols({ searchParams }: { searchParams: Search }) {
  const params = await searchParams;
  const q = one(params.q);
  const kind = one(params.kind);
  const area = one(params.area);
  const implementation = one(params.implementation);
  const codolOnly = one(params.codol) === '1';
  const offset = Number.parseInt(one(params.offset) || '0', 10) || 0;
  const limit = 50;

  const [result, areas] = await Promise.all([
    api<Paged<SymbolSummary>>(
      '/reference/symbols' +
        query({ q, kind, area, implementation, codol_only: codolOnly, limit, offset }),
    ),
    api<Area[]>('/reference/areas'),
  ]);

  const base = (next: number) =>
    '/reference/symbols' +
    query({ q, kind, area, implementation, codol: codolOnly ? '1' : '', offset: next || '' });

  return (
    <main className="content">
      <div className="page-head">
        <h1>Declarations</h1>
        <p className="lede">
          Every struct, union, enum, class, typedef, function, prototype and macro the indexer found,
          with the file and line it was declared on.
        </p>
      </div>

      <SymbolFilters areas={areas} />

      <p className="muted small">
        {result.total.toLocaleString('en-US')} matching {result.total === 1 ? 'entry' : 'entries'}
        {result.total > limit && (
          <>
            {' '}
            · showing {offset + 1}–{Math.min(offset + limit, result.total)}
          </>
        )}
      </p>

      {result.items.length === 0 ? (
        <div className="empty">Nothing matched those filters.</div>
      ) : (
        <div className="table-wrap">
          <table className="table">
            <thead>
              <tr>
                <th>Name</th>
                <th>Kind</th>
                <th>Area</th>
                <th>Signature</th>
                <th>Where</th>
              </tr>
            </thead>
            <tbody>
              {result.items.map((symbol) => (
                <tr key={symbol.id}>
                  <td className="mono">
                    <Link href={`/reference/symbols/${symbol.id}`}>{symbol.name}</Link>
                  </td>
                  <td>
                    <span className="badge">{symbol.kind}</span>{' '}
                    <ImplementationBadge value={symbol.implementation} />
                  </td>
                  <td>
                    <Link href={`/reference/areas/${symbol.area}`} className="faint">
                      {symbol.area}
                    </Link>
                  </td>
                  <td className="mono faint" style={{ maxWidth: 460 }}>
                    {symbol.signature.length > 150
                      ? `${symbol.signature.slice(0, 150)}…`
                      : symbol.signature}
                  </td>
                  <td className="mono faint">
                    {symbol.file}:{symbol.line}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}

      <div className="pagination">
        {offset > 0 && (
          <Link className="button" href={base(Math.max(offset - limit, 0))}>
            ← Previous
          </Link>
        )}
        {offset + limit < result.total && (
          <Link className="button" href={base(offset + limit)}>
            Next →
          </Link>
        )}
      </div>
    </main>
  );
}
