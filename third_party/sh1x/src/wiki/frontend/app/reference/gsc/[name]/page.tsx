import Link from 'next/link';
import { notFound } from 'next/navigation';
import type { Metadata } from 'next';
import { apiOrNull, query } from '@/lib/api';
import { ImplementationBadge } from '@/components/badges';
import type { Builtin, Paged, SymbolSummary } from '@/lib/types';

export const dynamic = 'force-dynamic';

type Props = { params: Promise<{ name: string }> };

export async function generateMetadata({ params }: Props): Promise<Metadata> {
  return { title: `${decodeURIComponent((await params).name)} (GSC)` };
}

export default async function BuiltinPage({ params }: Props) {
  const name = decodeURIComponent((await params).name);
  const rows = await apiOrNull<Builtin[]>(`/reference/builtins/${encodeURIComponent(name)}`);
  if (!rows?.length) notFound();

  const primary = rows[0];
  const binding = await apiOrNull<Paged<SymbolSummary>>(
    '/reference/symbols' + query({ q: primary.binding, limit: 10 }),
  );
  const definition = binding?.items.find(
    (symbol) => symbol.name === primary.binding && symbol.kind === 'function',
  );

  return (
    <main className="content narrow">
      <div className="page-head">
        <div className="crumbs">
          <Link href="/reference">reference</Link> /{' '}
          <Link href="/reference/gsc">GSC builtins</Link>
        </div>
        <h1 className="mono" style={{ fontSize: 24 }}>
          {primary.name}
        </h1>
        <p className="lede mono small">
          {primary.kind === 'method'
            ? `<entity> ${primary.name}( ... )`
            : `${primary.name}( ... )`}
        </p>
        <div className="row" style={{ marginTop: 8 }}>
          <span className="badge">{primary.kind}</span>
          <ImplementationBadge value={primary.implementation} />
          {primary.developer_only && (
            <span className="badge warn" title="Registered only when the developer flag is set.">
              developer only
            </span>
          )}
          {primary.area && <span className="badge">{primary.area}</span>}
        </div>
      </div>

      {primary.doc && (
        <div className="card" style={{ marginBottom: 18, whiteSpace: 'pre-wrap' }}>
          {primary.doc}
        </div>
      )}

      <h2 style={{ fontSize: 15 }}>Native binding</h2>
      <div className="table-wrap" style={{ marginBottom: 20 }}>
        <table className="table">
          <tbody>
            <tr>
              <th style={{ width: 160 }}>Bound symbol</th>
              <td className="mono">
                {definition ? (
                  <Link href={`/reference/symbols/${definition.id}`}>{primary.binding}</Link>
                ) : (
                  primary.binding
                )}
              </td>
            </tr>
            <tr>
              <th>Registered at</th>
              <td className="mono faint">
                {primary.registered_file}:{primary.registered_line}
              </td>
            </tr>
            <tr>
              <th>Defined at</th>
              <td className="mono faint">
                {primary.binding_file
                  ? `${primary.binding_file}:${primary.binding_line}`
                  : 'no definition found'}
              </td>
            </tr>
            <tr>
              <th>Developer flag</th>
              <td className="mono faint">{primary.developer_only ? 'set' : 'clear'}</td>
            </tr>
          </tbody>
        </table>
      </div>

      {primary.signature && (
        <>
          <h2 style={{ fontSize: 15 }}>Native signature</h2>
          <pre className="code" style={{ marginBottom: 20 }}>
            {primary.signature}
          </pre>
          <p className="faint small">
            GSC arguments do not appear in this signature: a builtin reads them from the VM stack
            with <code>Scr_GetInt</code>, <code>Scr_GetString</code>, <code>Scr_GetEntity</code> and
            friends, and returns by pushing with <code>Scr_AddInt</code> and its siblings. Read the
            body to see the real argument contract.
          </p>
        </>
      )}

      {primary.implementation === 'x86-asm' && (
        <div className="notice">
          This builtin runs retained x86 from before the fork: the binding lives in{' '}
          <code>{primary.binding_file}</code> as reassembled disassembly of the original game binary,
          so there is no C source to read for its behaviour.
        </div>
      )}

      {rows.length > 1 && (
        <section style={{ marginTop: 22 }}>
          <h2 style={{ fontSize: 15 }}>Registered more than once</h2>
          <div className="list">
            {rows.map((row) => (
              <div className="list-item small mono" key={row.id}>
                {row.kind} → {row.binding} ({row.registered_file}:{row.registered_line})
              </div>
            ))}
          </div>
          <p className="faint small">
            The same name can be registered as both a function and a method, or registered under a
            build flag. The last registration to run is the one the VM resolves.
          </p>
        </section>
      )}
    </main>
  );
}
