'use client';

export default function Error({ error, reset }: { error: Error; reset: () => void }) {
  return (
    <main className="content narrow">
      <div className="page-head">
        <h1>Something broke</h1>
        <p className="lede">
          The wiki could not render this page. If the API is down, start the backend and try again.
        </p>
      </div>
      <pre className="code">{error.message}</pre>
      <button className="button" style={{ marginTop: 12 }} onClick={reset}>
        Try again
      </button>
    </main>
  );
}
