import Link from 'next/link';
import { api, query } from '@/lib/api';
import type { Area, SourceFile } from '@/lib/types';

export const dynamic = 'force-dynamic';
export const metadata = { title: 'Source files' };

type Search = Promise<Record<string, string | string[] | undefined>>;

export default async function Files({ searchParams }: { searchParams: Search }) {
  const params = await searchParams;
  const areaFilter = Array.isArray(params.area) ? params.area[0] : (params.area ?? '');

  // Without a filter this is ~1,400 rows, which is a slow page for little
  // benefit; the area chips are the intended way in.
  const limit = areaFilter ? 2000 : 300;
  const [files, areas] = await Promise.all([
    api<SourceFile[]>('/reference/files' + query({ area: areaFilter, limit })),
    api<Area[]>('/reference/areas'),
  ]);
  const truncated = !areaFilter && files.length >= limit;

  return (
    <main className="content">
      <div className="page-head">
        <h1>Source files</h1>
        <p className="lede">
          Every file the indexer read, with its content hash. The hash is what makes a page&apos;s
          claims checkable: if it changed, the note above it may be stale.
        </p>
      </div>

      <div className="chip-row" style={{ marginBottom: 14 }}>
        <Link className={areaFilter ? 'badge' : 'badge accent'} href="/reference/files">
          all
        </Link>
        {areas.map((area) => (
          <Link
            key={area.name}
            className={areaFilter === area.name ? 'badge accent' : 'badge'}
            href={`/reference/files?area=${area.name}`}
          >
            {area.name}
          </Link>
        ))}
      </div>

      <p className="muted small">
        {files.length.toLocaleString('en-US')} files
        {truncated && ' — the first of every area. Pick an area above to see all of it.'}
      </p>

      <div className="table-wrap">
        <table className="table">
          <thead>
            <tr>
              <th>Path</th>
              <th>Area</th>
              <th>Role</th>
              <th>Lang</th>
              <th>Lines</th>
              <th>Declarations</th>
              <th>sha256</th>
            </tr>
          </thead>
          <tbody>
            {files.map((file) => (
              <tr key={file.path}>
                <td className="mono">
                  <Link href={`/reference/symbols?q=&area=${file.area}`}>{file.path}</Link>
                </td>
                <td className="faint small">{file.area}</td>
                <td className="faint small">{file.role}</td>
                <td className="faint small">{file.language}</td>
                <td className="mono">{file.lines.toLocaleString('en-US')}</td>
                <td className="mono">{file.symbol_count}</td>
                <td className="mono faint" title={file.sha256}>
                  {file.sha256.slice(0, 12)}
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </main>
  );
}
