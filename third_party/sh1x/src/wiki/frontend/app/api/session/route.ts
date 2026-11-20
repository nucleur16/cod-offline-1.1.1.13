import { NextResponse } from 'next/server';
import { API_URL, SESSION_COOKIE } from '@/lib/api';

/**
 * Exchanges a username and password for a session cookie.
 *
 * The token is set httpOnly, so page scripts cannot read it: a stored XSS in
 * rendered wiki markdown still cannot lift an editor's session.
 */
export async function POST(request: Request) {
  const body = await request.json().catch(() => null);
  if (!body?.username || !body?.password) {
    return NextResponse.json({ detail: 'Username and password are required.' }, { status: 400 });
  }

  const upstream = await fetch(`${API_URL}/api/auth/login`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ username: body.username, password: body.password }),
    cache: 'no-store',
  });
  const data = await upstream.json().catch(() => ({}));
  if (!upstream.ok) {
    return NextResponse.json(
      { detail: data?.detail ?? 'Sign-in failed.' },
      { status: upstream.status },
    );
  }

  const response = NextResponse.json({ user: data.user });
  response.cookies.set({
    name: SESSION_COOKIE,
    value: data.access_token,
    httpOnly: true,
    sameSite: 'lax',
    path: '/',
    maxAge: data.expires_in ?? 3600,
    secure: process.env.NODE_ENV === 'production',
  });
  return response;
}

export async function DELETE() {
  const response = NextResponse.json({ ok: true });
  response.cookies.set({ name: SESSION_COOKIE, value: '', path: '/', maxAge: 0 });
  return response;
}
