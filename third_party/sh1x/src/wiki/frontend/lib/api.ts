/**
 * Server-side access to the FastAPI backend.
 *
 * Only server components and route handlers import this: it reads the session
 * cookie directly, so the browser never has to hold a bearer token in
 * JavaScript. Client components mutate through `/api/proxy/...` instead.
 */
import { cookies } from 'next/headers';

export const SESSION_COOKIE = 'wiki_token';

export const API_URL =
  process.env.WIKI_API_URL?.replace(/\/$/, '') ?? 'http://127.0.0.1:8000';

export class ApiError extends Error {
  constructor(
    readonly status: number,
    message: string,
  ) {
    super(message);
    this.name = 'ApiError';
  }
}

async function authHeader(): Promise<Record<string, string>> {
  const token = (await cookies()).get(SESSION_COOKIE)?.value;
  return token ? { Authorization: `Bearer ${token}` } : {};
}

type Options = RequestInit & { auth?: boolean };

export async function api<T>(path: string, options: Options = {}): Promise<T> {
  const { auth = true, headers, ...rest } = options;
  const response = await fetch(`${API_URL}/api${path}`, {
    ...rest,
    headers: {
      'Content-Type': 'application/json',
      ...(auth ? await authHeader() : {}),
      ...(headers as Record<string, string>),
    },
    // The wiki is read-mostly but edits must show up immediately, so nothing
    // is cached at this layer; Next's router cache still covers navigation.
    cache: 'no-store',
  });
  if (!response.ok) {
    let detail = `${response.status} ${response.statusText}`;
    try {
      const body = await response.json();
      if (typeof body?.detail === 'string') detail = body.detail;
    } catch {
      /* a non-JSON error body is not worth reporting verbatim */
    }
    throw new ApiError(response.status, detail);
  }
  if (response.status === 204) return undefined as T;
  return (await response.json()) as T;
}

/** Returns `null` instead of throwing when the resource is simply absent. */
export async function apiOrNull<T>(path: string, options: Options = {}): Promise<T | null> {
  try {
    return await api<T>(path, options);
  } catch (error) {
    if (error instanceof ApiError && error.status === 404) return null;
    throw error;
  }
}

export function query(params: Record<string, string | number | boolean | undefined | null>): string {
  const search = new URLSearchParams();
  for (const [key, value] of Object.entries(params)) {
    if (value === undefined || value === null || value === '') continue;
    search.set(key, String(value));
  }
  const text = search.toString();
  return text ? `?${text}` : '';
}
