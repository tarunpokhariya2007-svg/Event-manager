/**
 * Central API configuration. The backend URL comes from VITE_API_BASE_URL
 * (see .env.example); nothing else in the frontend hardcodes a host.
 */
const rawBase = (import.meta.env.VITE_API_BASE_URL as string | undefined) ?? 'http://localhost:8080';

export const API_BASE_URL: string = rawBase.replace(/\/+$/, '');

const rawTimeout = Number(import.meta.env.VITE_API_TIMEOUT_MS);
export const API_TIMEOUT_MS: number = Number.isFinite(rawTimeout) && rawTimeout > 0 ? rawTimeout : 10000;

/** Google OAuth client id (public). Empty = Google sign-in is shown as unavailable. */
export const GOOGLE_CLIENT_ID: string = ((import.meta.env.VITE_GOOGLE_CLIENT_ID as string | undefined) ?? '').trim();

const V1 = `${API_BASE_URL}/api/v1`;

export const API_ENDPOINTS = {
  tasks: `${V1}/tasks`,
  events: `${V1}/events`,
  optimize: `${V1}/schedule/optimize`,
  health: `${V1}/schedule/optimize/health`,
  authRegister: `${V1}/auth/register`,
  authLogin: `${V1}/auth/login`,
  authGoogle: `${V1}/auth/google`,
  authMe: `${V1}/auth/me`,
  authLogout: `${V1}/auth/logout`,
} as const;
