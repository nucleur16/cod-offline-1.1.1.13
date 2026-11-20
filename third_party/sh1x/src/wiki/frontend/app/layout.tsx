import type { Metadata } from 'next';
import Link from 'next/link';
import './globals.css';
import { Sidebar } from '@/components/Sidebar';
import { SearchBox } from '@/components/SearchBox';
import { ThemeToggle } from '@/components/ThemeToggle';
import { currentUser, canEdit } from '@/lib/session';

export const metadata: Metadata = {
  title: {
    default: 'sh1x server wiki',
    template: '%s · sh1x server wiki',
  },
  description:
    'Engineering reference for the sh1x server, a CoD4x fork rebuilt for Call of Duty Online: subsystems, structs, functions and the GSC builtin surface.',
};

export const dynamic = 'force-dynamic';

export default async function RootLayout({ children }: { children: React.ReactNode }) {
  const user = await currentUser();
  return (
    <html lang="en" suppressHydrationWarning>
      <body>
        <div className="shell">
          <Sidebar />
          <div className="main">
            <header className="topbar">
              <SearchBox />
              <div className="spacer" />
              {canEdit(user) && (
                <Link className="button" href="/new">
                  New page
                </Link>
              )}
              <ThemeToggle />
              {user ? (
                <Link className="button" href="/account">
                  {user.display_name || user.username}
                </Link>
              ) : (
                <Link className="button" href="/login">
                  Sign in
                </Link>
              )}
            </header>
            {children}
          </div>
        </div>
      </body>
    </html>
  );
}
