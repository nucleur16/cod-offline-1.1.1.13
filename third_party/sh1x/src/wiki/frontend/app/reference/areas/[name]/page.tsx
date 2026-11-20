import Link from 'next/link';
import { notFound } from 'next/navigation';
import type { Metadata } from 'next';
import { api, apiOrNull, query } from '@/lib/api';
import { ImplementationBadge } from '@/components/badges';
import type { Area, Paged, PageSummary, SourceFile, SymbolSummary } from '@/lib/types';

export const dynamic = 'force-dynamic';

type Props = { params: Promise<{ name: string }> };

export async function generateMetadata({ params }: Props): Promise<Metadata> {
  const area = await apiOrNull<Area>(`/reference/areas/${(await params).name}`);
  return area ? { title: area.title } : { title: 'Not found' };
}

const TYPE_KINDS = 'struct,union,enum,class,typedef';

export default async function AreaPage({ params }: Props) {
  const name = (await params).name;
  const area = await apiOrNull<Area>(`/reference/areas/${name}`);
  if (!area) notFound();

  const [files, types, functions, pages] = await Promise.all([
    api<SourceFile[]>('/reference/files' + query({ area: name, limit: 600 })),
    api<Paged<SymbolSummary>>(
      '/reference/symbols' + query({ area: name, kind: TYPE_KINDS, limit: 200 }),
    ),
    api<Paged<SymbolSummary>>(
      '/reference/symbols' + query({ area: name, kind: 'prototype', limit: 200 }),
    ),
    api<Paged<PageSummary>>('/pages' + query({ limit: 500 })).catch(() => null),
  ]);

  const related = (pages?.items ?? []).filter((page) => page.tags?.includes(name));
  const headers = files.filter((file) => file.role === 'header');
  const sources = files.filter((file) => file.role !== 'header');

  return (
    <main className="content">
      <div className="page-head">
        <div className="crumbs">
          <Link href="/reference">reference</Link> / areas / {area.name}
        </div>
        <h1>{area.title}</h1>
        {area.summary && <p className="lede">{area.summary}</p>}
        <div className="row small faint" style={{ marginTop: 8 }}>
          <span className="mono">{area.path}</span>
          <span>{area.file_count} files</span>
          <span>{area.line_count.toLocaleString('en-US')} lines</span>
          <span>{area.symbol_count.toLocaleString('en-US')} declarations</span>
          {area.is_codol && <span className="badge accent">CoDOL</span>}
        </div>
      </div>

      {related.length > 0 && (
        <div className="card" style={{ marginBottom: 20 }}>
          <strong className="small">Written up in</strong>
          <div className="chip-row" style={{ marginTop: 6 }}>
            {related.map((page) => (
              <Link className="badge accent" key={page.slug} href={`/wiki/${page.slug}`}>
                {page.title}
              </Link>
            ))}
          </div>
        </div>
      )}

      <section style={{ marginBottom: 24 }}>
        <h2 style={{ fontSize: 16 }}>Types ({types.total})</h2>
        {types.items.length === 0 ? (
          <p className="faint small">No aggregate types declared in this area.</p>
        ) : (
          <div className="table-wrap">
            <table className="table">
              <thead>
                <tr>
                  <th>Name</th>
                  <th>Kind</th>
                  <th>Declared in</th>
                </tr>
              </thead>
              <tbody>
                {types.items.map((symbol) => (
                  <tr key={symbol.id}>
                    <td className="mono">
                      <Link href={`/reference/symbols/${symbol.id}`}>{symbol.name}</Link>
                    </td>
                    <td>
                      <span className="badge">{symbol.kind}</span>
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
        {types.total > types.items.length && (
          <p className="small">
            <Link href={`/reference/symbols?area=${name}&kind=struct`}>
              Browse all {types.total} declarations →
            </Link>
          </p>
        )}
      </section>

      <section style={{ marginBottom: 24 }}>
        <h2 style={{ fontSize: 16 }}>Public entry points ({functions.total})</h2>
        <p className="faint small">Functions declared in this area&apos;s headers.</p>
        <div className="table-wrap">
          <table className="table">
            <thead>
              <tr>
                <th>Signature</th>
                <th>Implementation</th>
                <th>Header</th>
              </tr>
            </thead>
            <tbody>
              {functions.items.map((symbol) => (
                <tr key={symbol.id}>
                  <td className="mono">
                    <Link href={`/reference/symbols/${symbol.id}`}>{symbol.signature}</Link>
                  </td>
                  <td>
                    <ImplementationBadge value={symbol.implementation} />
                  </td>
                  <td className="mono faint">
                    {symbol.file}:{symbol.line}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </section>

      <section>
        <h2 style={{ fontSize: 16 }}>Files</h2>
        <div className="grid two">
          <div>
            <p className="nav-heading">Headers</p>
            <div className="list">
              {headers.map((file) => (
                <div className="list-item small mono" key={file.path}>
                  {file.path}
                  <span className="faint"> · {file.lines} lines</span>
                </div>
              ))}
              {!headers.length && <div className="list-item faint small">None.</div>}
            </div>
          </div>
          <div>
            <p className="nav-heading">Sources</p>
            <div className="list">
              {sources.map((file) => (
                <div className="list-item small mono" key={file.path}>
                  {file.path}
                  <span className="faint">
                    {' '}
                    · {file.lines} lines · {file.language}
                  </span>
                </div>
              ))}
              {!sources.length && <div className="list-item faint small">None.</div>}
            </div>
          </div>
        </div>
      </section>
    </main>
  );
}
