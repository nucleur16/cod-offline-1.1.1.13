import { apiOrNull } from './api';
import type { User } from './types';

/** The signed-in user for the current request, or `null` when signed out. */
export async function currentUser(): Promise<User | null> {
  try {
    return await apiOrNull<User>('/auth/me');
  } catch {
    // An expired or malformed cookie should render the signed-out wiki, not an
    // error page.
    return null;
  }
}

export function canEdit(user: User | null): boolean {
  return !!user && user.is_active && (user.role === 'admin' || user.role === 'editor');
}

export function isAdmin(user: User | null): boolean {
  return !!user && user.is_active && user.role === 'admin';
}
