'use client';

import Link from 'next/link';
import { usePathname } from 'next/navigation';

/** A sidebar link that knows when it is the page you are on. */
export function NavLink({ href, children }: { href: string; children: React.ReactNode }) {
  const pathname = usePathname();
  const current = pathname === href || pathname.startsWith(`${href}/`);
  return (
    <Link className={current ? 'nav-link current' : 'nav-link'} href={href}>
      {children}
    </Link>
  );
}
