'use client';

import { useEffect, useState } from 'react';

const KEY = 'sh1x-wiki-theme';

/** Dark by default; the choice is this browser's, so localStorage fits. */
export function ThemeToggle() {
  const [theme, setTheme] = useState<'dark' | 'light'>('dark');

  useEffect(() => {
    try {
      const stored = localStorage.getItem(KEY);
      if (stored === 'light' || stored === 'dark') {
        setTheme(stored);
        document.documentElement.dataset.theme = stored;
      }
    } catch {
      /* private mode: keep the default */
    }
  }, []);

  function toggle() {
    const next = theme === 'dark' ? 'light' : 'dark';
    setTheme(next);
    document.documentElement.dataset.theme = next;
    try {
      localStorage.setItem(KEY, next);
    } catch {
      /* nothing to do if storage is blocked */
    }
  }

  return (
    <button className="button" onClick={toggle} title="Switch theme" aria-label="Switch theme">
      {theme === 'dark' ? '☾' : '☀'}
    </button>
  );
}
