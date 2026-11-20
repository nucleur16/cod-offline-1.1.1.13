'use client';

import { useRouter } from 'next/navigation';
import { useState } from 'react';
import { request } from '@/lib/client';

/** Restores an earlier version, keeping the current one as a new revision. */
export function RevertButton({ slug, version }: { slug: string; version: number }) {
  const router = useRouter();
  const [busy, setBusy] = useState(false);

  async function revert() {
    if (!confirm(`Restore version ${version}? The current text is kept as a revision.`)) return;
    setBusy(true);
    try {
      await request(`/pages/${slug}/revert/${version}`, { method: 'POST' });
      router.push(`/wiki/${slug}`);
      router.refresh();
    } catch (error) {
      alert(error instanceof Error ? error.message : String(error));
      setBusy(false);
    }
  }

  return (
    <button className="button" onClick={revert} disabled={busy}>
      Restore
    </button>
  );
}
