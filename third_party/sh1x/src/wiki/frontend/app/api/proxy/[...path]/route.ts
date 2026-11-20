import { cookies } from 'next/headers';
import { NextResponse } from 'next/server';
import { API_URL, SESSION_COOKIE } from '@/lib/api';

/**
 * Forwards a browser call to the backend with the session cookie attached.
 *
 * Client components (the editor, the admin screens) mutate through here rather
 * than calling the API directly, so the token stays httpOnly and the backend
 * never needs to be exposed to the browser at all.
 */
async function forward(request: Request, path: string[]) {
  const token = (await cookies()).get(SESSION_COOKIE)?.value;
  const url = new URL(request.url);
  const target = `${API_URL}/api/${path.join('/')}${url.search}`;

  const headers: Record<string, string> = { 'Content-Type': 'application/json' };
  if (token) headers.Authorization = `Bearer ${token}`;

  const method = request.method.toUpperCase();
  const body = method === 'GET' || method === 'HEAD' ? undefined : await request.text();

  const upstream = await fetch(target, { method, headers, body, cache: 'no-store' });
  if (upstream.status === 204) return new NextResponse(null, { status: 204 });

  const text = await upstream.text();
  try {
    return NextResponse.json(text ? JSON.parse(text) : null, { status: upstream.status });
  } catch {
    return new NextResponse(text, { status: upstream.status });
  }
}

type Context = { params: Promise<{ path: string[] }> };

export async function GET(request: Request, context: Context) {
  return forward(request, (await context.params).path);
}

export async function POST(request: Request, context: Context) {
  return forward(request, (await context.params).path);
}

export async function PATCH(request: Request, context: Context) {
  return forward(request, (await context.params).path);
}

export async function PUT(request: Request, context: Context) {
  return forward(request, (await context.params).path);
}

export async function DELETE(request: Request, context: Context) {
  return forward(request, (await context.params).path);
}
