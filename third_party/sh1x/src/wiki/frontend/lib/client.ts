'use client';

/** Browser-side calls, routed through the Next proxy so the token stays httpOnly. */
export async function request<T>(path: string, init: RequestInit = {}): Promise<T> {
  const response = await fetch(`/api/proxy${path}`, {
    ...init,
    headers: { 'Content-Type': 'application/json', ...(init.headers ?? {}) },
  });
  if (!response.ok) {
    let detail = `${response.status} ${response.statusText}`;
    try {
      const body = await response.json();
      if (typeof body?.detail === 'string') detail = body.detail;
      else if (Array.isArray(body?.detail)) detail = body.detail.map(describe).join('; ');
    } catch {
      /* keep the status line */
    }
    throw new Error(detail);
  }
  if (response.status === 204) return undefined as T;
  return (await response.json()) as T;
}

/** Turns one FastAPI validation error into something a human can act on. */
function describe(error: { loc?: (string | number)[]; msg?: string }): string {
  const field = (error.loc ?? []).filter((part) => part !== 'body').join('.');
  return field ? `${field}: ${error.msg}` : String(error.msg ?? 'invalid value');
}

export async function signIn(username: string, password: string): Promise<void> {
  const response = await fetch('/api/session', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ username, password }),
  });
  if (!response.ok) {
    const body = await response.json().catch(() => ({}));
    throw new Error(body?.detail ?? 'Sign-in failed.');
  }
}

export async function signOut(): Promise<void> {
  await fetch('/api/session', { method: 'DELETE' });
}
