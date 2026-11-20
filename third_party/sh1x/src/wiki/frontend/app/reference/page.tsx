import Link from 'next/link';
import { api } from '@/lib/api';
import type { Area, SiteStats } from '@/lib/types';

export const dynamic = 'force-dynamic';
export const metadata = { title: 'Reference' };

function number(value: number): string {
  return value.toLocaleString('en-US');
}

export default async function Reference() {
  const [areas, stats] = await Promise.all([
    api<Area[]>('/reference/areas'),
    api<SiteStats>('/site'),
  ]);

  return (
    <main className="content">
      <div className="page-head">
        <h1>Source reference</h1>
        <p className="lede">
          Built by reading every C, C++ and assembly file in the tree. It reports what the source
          says, not what the compiler resolves: nothing here is preprocessed, so a macro-generated
          declaration is invisible and a declaration inside a disabled <code>#ifdef</code> is still
          listed. Every entry carries its file and line.
        </p>
      </div>

      <div className="grid three" style={{ marginBottom: 24 }}>
        <Link className="card" href="/reference/symbols">
          <strong>Declarations</strong>
          <p className="muted small" style={{ margin: '4px 0 0' }}>
            {number(stats.symbols)} structs, unions, enums, classes, typedefs, functions and macros.
          </p>
        </Link>
        <Link className="card" href="/reference/gsc">
          <strong>GSC builtins</strong>
          <p className="muted small" style={{ margin: '4px 0 0' }}>
            {number(stats.builtins)} names scripts can call, each linked to the native function it
            binds to.
          </p>
        </Link>
        <Link className="card" href="/reference/files">
          <strong>Source files</strong>
          <p className="muted small" style={{ margin: '4px 0 0' }}>
            {number(stats.files)} files with their area, language, size and content hash.
          </p>
        </Link>
      </div>

      <h2 style={{ fontSize: 17 }}>How functions are implemented</h2>
      <p className="muted small" style={{ maxWidth: '78ch' }}>
        CoD4x was forked from a server that had never been fully rewritten. Most engine functions
        from before the fork are still bare x86: disassembled from the shipped game binary and
        reassembled under their own names in <code>src/asmsource/</code>. The wiki marks those, so
        when a function has no C source to read you know why.
      </p>
      <div className="grid three" style={{ margin: '12px 0 26px' }}>
        {Object.entries(stats.implementations)
          .sort((a, b) => b[1] - a[1])
          .map(([key, count]) => (
            <Link
              className="stat"
              key={key}
              href={`/reference/symbols?implementation=${encodeURIComponent(key)}`}
            >
              <div className="value">{number(count)}</div>
              <div className="label">
                {key === 'x86-asm'
                  ? 'x86 assembly'
                  : key === 'cpp'
                    ? 'C++'
                    : key === 'c'
                      ? 'C'
                      : 'no definition found'}
              </div>
            </Link>
          ))}
      </div>

      <h2 style={{ fontSize: 17 }}>Areas</h2>
      <div className="table-wrap">
        <table className="table">
          <thead>
            <tr>
              <th>Area</th>
              <th className="mono">path</th>
              <th>Files</th>
              <th>Lines</th>
              <th>Declarations</th>
              <th>Languages</th>
            </tr>
          </thead>
          <tbody>
            {areas.map((area) => (
              <tr key={area.name}>
                <td>
                  <Link href={`/reference/areas/${area.name}`}>{area.title}</Link>
                  {area.is_codol && <span className="badge accent"> codol</span>}
                </td>
                <td className="mono faint">{area.path}</td>
                <td className="mono">{area.file_count}</td>
                <td className="mono">{number(area.line_count)}</td>
                <td className="mono">{number(area.symbol_count)}</td>
                <td className="mono faint">
                  {Object.entries(area.languages)
                    .map(([language, count]) => `${language} ${count}`)
                    .join(' · ')}
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </main>
  );
}
