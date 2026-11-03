import { API_TIMEOUT_MS } from '../config/api';

export type ApiErrorKind = 'network' | 'timeout' | 'http' | 'invalid_json' | 'invalid_response';

/** Error thrown for every failed backend call. `message` is safe to show to the user. */
export class ApiError extends Error {
  kind: ApiErrorKind;
  status?: number;
  code?: string;

  constructor(kind: ApiErrorKind, message: string, status?: number, code?: string) {
    super(message);
    this.name = 'ApiError';
    this.kind = kind;
    this.status = status;
    this.code = code;
  }
}

export function errorMessage(err: unknown): string {
  if (err instanceof ApiError) return err.message;
  return err instanceof Error ? err.message : 'Unexpected error.';
}

/** CSRF token for state-changing requests; handed out by the auth endpoints, kept in memory only. */
let csrfToken: string | null = null;
export function setCsrfToken(token: string | null): void {
  csrfToken = token;
}

/** Called when a non-auth request is answered with 401 (session expired or revoked). */
let unauthorizedHandler: (() => void) | null = null;
export function setUnauthorizedHandler(handler: (() => void) | null): void {
  unauthorizedHandler = handler;
}

/**
 * JSON request against the backend. Backend error bodies have the shape
 * { error: { code, message } } (API_CONTRACT.md §2).
 */
export async function apiRequest<T>(
  url: string,
  options: {
    method?: 'GET' | 'POST' | 'PUT' | 'DELETE';
    body?: unknown;
    timeoutMs?: number;
    /** Sign-in/sign-up calls: a 401 means "wrong credentials", not "session expired". */
    isAuthRequest?: boolean;
  } = {}
): Promise<T> {
  const { method = 'GET', body, timeoutMs = API_TIMEOUT_MS, isAuthRequest = false } = options;
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), timeoutMs);

  let res: Response;
  try {
    res = await fetch(url, {
      method,
      credentials: 'include', // the session lives in an HttpOnly cookie
      headers: {
        Accept: 'application/json',
        ...(body !== undefined ? { 'Content-Type': 'application/json' } : {}),
        ...(method !== 'GET' && csrfToken ? { 'X-CSRF-Token': csrfToken } : {}),
      },
      body: body !== undefined ? JSON.stringify(body) : undefined,
      signal: controller.signal,
    });
  } catch (e) {
    if (e instanceof DOMException && e.name === 'AbortError') {
      throw new ApiError('timeout', `The C++ backend did not respond within ${Math.round(timeoutMs / 1000)}s.`);
    }
    throw new ApiError(
      'network',
      'Cannot reach the C++ backend. Make sure it is running and that VITE_API_BASE_URL and its CORS origin are correct.'
    );
  } finally {
    clearTimeout(timer);
  }

  if (res.status === 204) return undefined as T;

  const text = await res.text();
  let json: unknown = undefined;
  if (text.length > 0) {
    try {
      json = JSON.parse(text);
    } catch {
      if (res.ok) throw new ApiError('invalid_json', 'The backend returned invalid JSON.', res.status);
    }
  }

  if (res.status === 401 && !isAuthRequest && unauthorizedHandler) unauthorizedHandler();

  if (!res.ok) {
    const err = (json as { error?: { code?: string; message?: string } } | undefined)?.error;
    const message = err?.message ?? `Backend request failed (HTTP ${res.status}).`;
    throw new ApiError('http', message, res.status, err?.code);
  }
  return json as T;
}
