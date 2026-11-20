'use client';

import { useRouter, useSearchParams } from 'next/navigation';
import { useEffect, useRef, useState } from 'react';

export function SearchBox() {
  const router = useRouter();
  const params = useSearchParams();
  const [value, setValue] = useState(params.get('q') ?? '');
  const input = useRef<HTMLInputElement>(null);

  useEffect(() => {
    // "/" focuses search the way every code host does, but not while the user
    // is already typing into a field.
    function onKey(event: KeyboardEvent) {
      const target = event.target as HTMLElement | null;
      const typing = target && /^(INPUT|TEXTAREA|SELECT)$/.test(target.tagName);
      if (event.key === '/' && !typing) {
        event.preventDefault();
        input.current?.focus();
      }
    }
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, []);

  return (
    <form
      className="search-form"
      onSubmit={(event) => {
        event.preventDefault();
        if (value.trim()) router.push(`/search?q=${encodeURIComponent(value.trim())}`);
      }}
    >
      <input
        ref={input}
        type="search"
        value={value}
        onChange={(event) => setValue(event.target.value)}
        placeholder="Search pages, structs, functions, GSC builtins    /"
        aria-label="Search the wiki"
      />
    </form>
  );
}
