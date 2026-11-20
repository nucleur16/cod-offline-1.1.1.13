import Link from 'next/link';
import { api, query } from '@/lib/api';
import { ImplementationBadge } from '@/components/badges';
import { BuiltinFilters } from '@/components/BuiltinFilters';
import type { Area, Builtin, Paged } from '@/lib/types';

export const dynamic = 'force-dynamic';
export const metadata = { title: 'GSC builtins' };

type Search = Promise<Record<string, string | string[] | undefined>>;

function one(value: string | string[] | undefined): string {
  return Array.isArray(value) ? (value[0] ?? '') : (value ?? '');
}

export default async function GscIndex({ searchParams }: { searchParams: Search }) {
  const params = await searchParams;
  const q = one(params.q);
  const kind = one(params.kind);
  const area = one(params.area);
  const implementation = one(params.implementation);
  const offset = Number.parseInt(one(params.offset) || '0', 10) || 0;
  const limit = 100;

  const [result, areas] = await Promise.all([
    api<Paged<Builtin>>(
      '/reference/builtins' + query({ q, kind, area, implementation, limit, offset }),
    ),
    api<Area[]>('/reference/areas'),
  ]);

  const href = (next: number) =>
    '/reference/gsc' + query({ q, kind, area, implementation, offset: next || '' });

  return (
    <main className="content">
      <div className="page-head">
        <h1>GSC builtins</h1>
        <p className="lede">
          Names a script can call. A <strong>function</strong> is called bare —{' '}
          <code>getent(name, key)</code> — and a <strong>method</strong> is called on a receiver —{' '}
          <code>player giveWeapon(...)</code>. Each one is registered with{' '}
          <code>Scr_AddFunction</code> or <code>Scr_AddMethod</code> and bound to a native symbol.
        </p>
      </div>

      <BuiltinFilters areas={areas} />

      <p className="muted small">
        {result.total.toLocaleString('en-US')} registered
        {result.total > limit && (
          <>
            {' '}
            · showing {offset + 1}–{Math.min(offset + limit, result.total)}
          </>
        )}
      </p>

      <div className="table-wrap">
        <table className="table">
          <thead>
            <tr>
              <th>Name</th>
              <th>Call</th>
              <th>Bound to</th>
              <th>Area</th>
              <th>Implementation</th>
            </tr>
          </thead>
          <tbody>
            {result.items.map((builtin) => (
              <tr key={builtin.id}>
                <td className="mono">
                  <Link href={`/reference/gsc/${builtin.name}`}>{builtin.name}</Link>
                  {builtin.developer_only && (
                    <span className="badge warn" title="Only registered when developer is set.">
                      {' '}
                      dev
                    </span>
                  )}
                </td>
                <td className="small">{builtin.kind}</td>
                <td className="mono faint">{builtin.binding}</td>
                <td className="faint small">{builtin.area || '—'}</td>
                <td>
                  <ImplementationBadge value={builtin.implementation} />
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>

      <div className="pagination">
        {offset > 0 && (
          <Link className="button" href={href(Math.max(offset - limit, 0))}>
            ← Previous
          </Link>
        )}
        {offset + limit < result.total && (
          <Link className="button" href={href(offset + limit)}>
            Next →
          </Link>
        )}
      </div>
    </main>
  );
}
