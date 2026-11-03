import { API_ENDPOINTS } from '../config/api';
import { apiRequest, setCsrfToken } from './apiClient';

export interface AuthUser {
  id: number;
  email: string;
  display_name: string;
  auth_methods: Array<'password' | 'google'>;
}

interface SessionResponse {
  user: AuthUser;
  csrf_token: string;
  created?: boolean;
}

async function startSession(request: Promise<SessionResponse>): Promise<AuthUser> {
  const res = await request;
  setCsrfToken(res.csrf_token);
  return res.user;
}

/** Thin wrappers over /api/v1/auth/*. The session cookie is set by the server, never read here. */
export const authService = {
  register: (email: string, password: string, displayName: string) =>
    startSession(
      apiRequest<SessionResponse>(API_ENDPOINTS.authRegister, {
        method: 'POST',
        body: { email, password, display_name: displayName },
        isAuthRequest: true,
      })
    ),

  login: (email: string, password: string) =>
    startSession(
      apiRequest<SessionResponse>(API_ENDPOINTS.authLogin, {
        method: 'POST',
        body: { email, password },
        isAuthRequest: true,
      })
    ),

  /** `credential` is the ID token from Google Identity Services; the server verifies it. */
  google: (credential: string) =>
    startSession(
      apiRequest<SessionResponse>(API_ENDPOINTS.authGoogle, {
        method: 'POST',
        body: { credential },
        isAuthRequest: true,
      })
    ),

  /** Restores the session after a page load (also re-issues the CSRF token). */
  me: () => startSession(apiRequest<SessionResponse>(API_ENDPOINTS.authMe, { isAuthRequest: true })),

  logout: async (): Promise<void> => {
    try {
      await apiRequest<void>(API_ENDPOINTS.authLogout, { method: 'POST', isAuthRequest: true });
    } finally {
      setCsrfToken(null);
    }
  },
};

/** Removes every ChronoPlan value cached in this browser so the next account starts clean. */
export function clearLocalUserData(): void {
  try {
    Object.keys(localStorage)
      .filter(key => key.startsWith('chronoplan_'))
      .forEach(key => localStorage.removeItem(key));
  } catch {
    /* storage may be unavailable (private mode) */
  }
}
