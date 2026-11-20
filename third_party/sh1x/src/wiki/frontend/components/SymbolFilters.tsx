'use client';

import { useRouter, useSearchParams } from 'next/navigation';
import { useState } from 'react';
import type { Area } from '@/lib/types';

const KINDS = ['struct', 'union', 'enum', 'class', 'typedef', 'function', 'prototype', 'macro'];
const IMPLEMENTATIONS = [
  ['c', 'C'],
  ['cpp', 'C++'],
  ['x86-asm', 'x86 assembly'],
  ['unresolved', 'no definition found'],
];

/** Filter bar for the declaration browser; state lives in the URL. */
export function SymbolFilters({ areas }: { areas: Area[] }) {
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
    router.push(`/reference/symbols?${next.toString()}`);
  }

  return (
    <form
      className="filters"
      onSubmit={(event) => {
        event.preventDefault();
        go({ q });
      }}
    >
      <div className="field" style={{ flex: 1, minWidth: 240 }}>
        <label htmlFor="q">Name or signature contains</label>
        <input id="q" value={q} onChange={(event) => setQ(event.target.value)} placeholder="CODOL_Write" />
      </div>
      <div className="field">
        <label htmlFor="kind">Kind</label>
        <select
          id="kind"
          value={params.get('kind') ?? ''}
          onChange={(event) => go({ kind: event.target.value })}
        >
          <option value="">Any</option>
          {KINDS.map((kind) => (
            <option key={kind} value={kind}>
              {kind}
            </option>
          ))}
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
          {IMPLEMENTATIONS.map(([value, label]) => (
            <option key={value} value={value}>
              {label}
            </option>
          ))}
        </select>
      </div>
      <button className="button" type="submit">
        Filter
      </button>
      <button
        className="button"
        type="button"
        onClick={() => go({ codol: params.get('codol') === '1' ? '' : '1' })}
      >
        {params.get('codol') === '1' ? 'CoDOL only ✓' : 'CoDOL only'}
      </button>
    </form>
  );
}
