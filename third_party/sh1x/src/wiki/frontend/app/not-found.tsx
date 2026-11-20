import Link from 'next/link';

export default function NotFound() {
  return (
    <main className="content narrow">
      <div className="page-head">
        <h1>Not here</h1>
        <p className="lede">
          There is no page, declaration or builtin at this address. It may have been renamed, or the
          reference may need reindexing after a source change.
        </p>
      </div>
      <div className="button-row">
        <Link className="button" href="/">Front page</Link>
        <Link className="button" href="/reference">Reference</Link>
      </div>
    </main>
  );
}
