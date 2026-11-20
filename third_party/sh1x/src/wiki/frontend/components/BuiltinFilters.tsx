'use client';

import { useRouter, useSearchParams } from 'next/navigation';
import { useState } from 'react';
import type { Area } from '@/lib/types';

/** Filter bar for the GSC builtin browser; state lives in the URL. */
export function BuiltinFilters({ areas }: { areas: Area[] }) {
  const router = useRouter();
  const params = useSearchParams();
  const [q, setQ] = useState(params.get('q') ?? '');

  function go(patch: Record<string, string>) {
    const next = new URLSearchParams(params.toString());
    for (const [key, value] of Object.entries(patch)) {
      if (value) next.set(key, value);
      else next.delete(key);
    }
    next.delete('offset');
    router.push(`/reference/gsc?${next.toString()}`);
  }

  return (
    <form
      className="filters"
      onSubmit={(event) => {
        event.preventDefault();
        go({ q });
      }}
    >
      <div className="field" style={{ flex: 1, minWidth: 220 }}>
        <label htmlFor="q">Name or binding contains</label>
        <input
          id="q"
          value={q}
          onChange={(event) => setQ(event.target.value)}
          placeholder="giveweapon"
        />
      </div>
      <div className="field">
        <label htmlFor="kind">Call style</label>
        <select
          id="kind"
          value={params.get('kind') ?? ''}
          onChange={(event) => go({ kind: event.target.value })}
        >
          <option value="">Any</option>
          <option value="function">function</option>
          <option value="method">method</option>
        </select>
      </div>
      <div className="field">
        <label htmlFor="area">Area</label>
        <select
          id="area"
          value={params.get('area') ?? ''}
          onChange={(event) => go({ area: event.target.value })}
        >
          <option value="">Any</option>
          {areas.map((area) => (
            <option key={area.name} value={area.name}>
              {area.name}
            </option>
          ))}
        </select>
      </div>
      <div className="field">
        <label htmlFor="implementation">Implemented in</label>
        <select
          id="implementation"
          value={params.get('implementation') ?? ''}
          onChange={(event) => go({ implementation: event.target.value })}
        >
          <option value="">Any</option>
          <option value="c">C</option>
          <option value="cpp">C++</option>
          <option value="x86-asm">x86 assembly</option>
          <option value="unresolved">no definition found</option>
        </select>
      </div>
      <button className="button" type="submit">
        Filter
      </button>
    </form>
  );
}
