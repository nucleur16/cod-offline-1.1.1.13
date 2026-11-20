import Link from 'next/link';
import { notFound } from 'next/navigation';
import type { Metadata } from 'next';
import { api, apiOrNull, query } from '@/lib/api';
import { ImplementationBadge } from '@/components/badges';
import type { Builtin, Paged, SymbolDetail, SymbolSummary } from '@/lib/types';

export const dynamic = 'force-dynamic';

type Props = { params: Promise<{ id: string }> };

export async function generateMetadata({ params }: Props): Promise<Metadata> {
  const symbol = await apiOrNull<SymbolDetail>(`/reference/symbols/id/${(await params).id}`);
  return symbol ? { title: symbol.name } : { title: 'Not found' };
}

export default async function SymbolPage({ params }: Props) {
  const symbol = await apiOrNull<SymbolDetail>(`/reference/symbols/id/${(await params).id}`);
  if (!symbol) notFound();

  const detail = symbol.detail ?? {};
  const [siblings, builtins] = await Promise.all([
    api<Paged<SymbolSummary>>(
      '/reference/symbols' + query({ file: symbol.file, limit: 200 }),
    ).catch(() => null),
    apiOrNull<Builtin[]>(`/reference/builtins/${encodeURIComponent(symbol.name)}`).catch(() => null),
  ]);
  const boundAs = (builtins ?? []).filter((builtin) => builtin.binding === symbol.name);

  return (
    <main className="content">
      <div className="page-head">
        <div className="crumbs">
          <Link href="/reference">reference</Link> /{' '}
          <Link href="/reference/symbols">declarations</Link> /{' '}
          <Link href={`/reference/areas/${symbol.area}`}>{symbol.area}</Link>
        </div>
        <h1 className="mono" style={{ fontSize: 24 }}>
          {symbol.name}
        </h1>
        <div className="row" style={{ marginTop: 6 }}>
          <span className="badge">{symbol.kind}</span>
          <ImplementationBadge value={symbol.implementation} />
          {symbol.is_codol && <span className="badge accent">CoDOL</span>}
          {detail.storage === 'static' && (
            <span className="badge" title="Not visible outside this translation unit.">
              static
            </span>
          )}
          {detail.exported && <span className="badge accent">global</span>}
          <span className="faint small mono">
            {symbol.file}:{symbol.line}
            {symbol.end_line > symbol.line ? `–${symbol.end_line}` : ''}
          </span>
        </div>
      </div>

      {symbol.doc && (
        <div className="card" style={{ marginBottom: 18, whiteSpace: 'pre-wrap' }}>
          {symbol.doc}
        </div>
      )}

      {detail.implementation_note && (
        <div className="notice" style={{ marginBottom: 18 }}>
          {detail.implementation_note}
        </div>
      )}

      <h2 style={{ fontSize: 15 }}>Signature</h2>
      <pre className="code" style={{ marginBottom: 20 }}>
        {symbol.signature}
      </pre>

      {detail.alias_of && (
        <p className="muted small">
          Alias of {detail.alias_of.kind}{' '}
          <code>{detail.alias_of.name}</code> declared at{' '}
          <span className="mono faint">
            {detail.alias_of.file}:{detail.alias_of.line}
          </span>
          .
        </p>
      )}
      {!!detail.aliases?.length && (
        <p className="muted small">
          Also known as {detail.aliases.map((alias) => <code key={alias}>{alias}</code>)}.
        </p>
      )}

      {!!detail.members?.length && (
        <section style={{ marginBottom: 22 }}>
          <h2 style={{ fontSize: 15 }}>Members</h2>
          <p className="faint small">
            In declaration order. No byte offsets are shown: this tree uses packing pragmas, so
            layout is a compiler question, not a textual one.
          </p>
          <div className="table-wrap">
            <table className="table">
              <thead>
                <tr>
                  <th>Type</th>
                  <th>Name</th>
                  <th>Note</th>
                </tr>
              </thead>
              <tbody>
                {detail.members.map((member, index) => (
                  <tr key={`${member.name}-${index}`}>
                    <td className="mono">{member.type}</td>
                    <td className="mono">
                      {member.name}
                      {member.bits && <span className="faint"> : {member.bits}</span>}
                    </td>
                    <td className="muted small">{member.comment}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </section>
      )}

      {!!detail.values?.length && (
        <section style={{ marginBottom: 22 }}>
          <h2 style={{ fontSize: 15 }}>Enumerators</h2>
          <div className="table-wrap">
            <table className="table">
              <thead>
                <tr>
                  <th>Name</th>
                  <th>Value</th>
                  <th>Note</th>
                </tr>
              </thead>
              <tbody>
                {detail.values.map((value, index) => (
                  <tr key={`${value.name}-${index}`}>
                    <td className="mono">{value.name}</td>
                    <td className="mono faint">{value.value || '—'}</td>
                    <td className="muted small">{value.comment}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </section>
      )}

      {!!detail.methods?.length && (
        <section style={{ marginBottom: 22 }}>
          <h2 style={{ fontSize: 15 }}>Member functions</h2>
          <pre className="code">{detail.methods.join('\n')}</pre>
        </section>
      )}

      {!!detail.params?.length && (
        <section style={{ marginBottom: 22 }}>
          <h2 style={{ fontSize: 15 }}>Parameters</h2>
          <div className="table-wrap">
            <table className="table">
              <thead>
                <tr>
                  <th>Type</th>
                  <th>Name</th>
                </tr>
              </thead>
              <tbody>
                {detail.params.map((param, index) => (
                  <tr key={index}>
                    <td className="mono">{param.type}</td>
                    <td className="mono faint">{param.name || '—'}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
          {detail.returns && (
            <p className="muted small" style={{ marginTop: 8 }}>
              Returns <code>{detail.returns}</code>.
            </p>
          )}
        </section>
      )}

      {!!detail.defined_in?.length && (
        <section style={{ marginBottom: 22 }}>
          <h2 style={{ fontSize: 15 }}>Defined in</h2>
          <div className="list">
            {detail.defined_in.map((site, index) => (
              <div className="list-item mono small" key={index}>
                {site.file}:{site.line}{' '}
                <ImplementationBadge value={site.language as SymbolDetail['implementation']} />
              </div>
            ))}
          </div>
        </section>
      )}

      {!!boundAs.length && (
        <section style={{ marginBottom: 22 }}>
          <h2 style={{ fontSize: 15 }}>Reachable from GSC</h2>
          <div className="list">
            {boundAs.map((builtin) => (
              <Link className="list-item" key={builtin.id} href={`/reference/gsc/${builtin.name}`}>
                <strong className="mono">{builtin.name}</strong>{' '}
                <span className="badge">{builtin.kind}</span>
                {builtin.developer_only && <span className="badge warn"> developer only</span>}
              </Link>
            ))}
          </div>
        </section>
      )}

      <section style={{ marginBottom: 22 }}>
        <h2 style={{ fontSize: 15 }}>Declaration as written</h2>
        <pre className="code">{symbol.declaration}</pre>
      </section>

      {siblings && siblings.items.length > 1 && (
        <section>
          <h2 style={{ fontSize: 15 }}>Also in {symbol.file}</h2>
          <div className="chip-row">
            {siblings.items
              .filter((other) => other.id !== symbol.id)
              .slice(0, 60)
              .map((other) => (
                <Link className="badge" key={other.id} href={`/reference/symbols/${other.id}`}>
                  {other.name}
                </Link>
              ))}
          </div>
        </section>
      )}
    </main>
  );
}
