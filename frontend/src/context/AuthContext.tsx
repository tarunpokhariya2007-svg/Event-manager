import React, { createContext, useCallback, useContext, useEffect, useMemo, useState } from 'react';
import { ApiError, errorMessage, setCsrfToken, setUnauthorizedHandler } from '../services/apiClient';
import { AuthUser, authService, clearLocalUserData } from '../services/authService';

type AuthStatus = 'loading' | 'authenticated' | 'anonymous' | 'unavailable';

interface AuthContextType {
  user: AuthUser | null;
  status: AuthStatus;
  /** Set when the backend could not be reached while restoring the session. */
  error: string | null;
  login: (email: string, password: string) => Promise<void>;
  register: (email: string, password: string, displayName: string) => Promise<void>;
  loginWithGoogle: (credential: string) => Promise<void>;
  logout: () => Promise<void>;
  retry: () => void;
}

const AuthContext = createContext<AuthContextType | undefined>(undefined);

export const AuthProvider: React.FC<{ children: React.ReactNode }> = ({ children }) => {
  const [user, setUser] = useState<AuthUser | null>(null);
  const [status, setStatus] = useState<AuthStatus>('loading');
  const [error, setError] = useState<string | null>(null);
  const [attempt, setAttempt] = useState(0);

  // Page load / refresh: ask the server who the cookie belongs to.
  useEffect(() => {
    let cancelled = false;
    setStatus('loading');
    authService
      .me()
      .then(u => {
        if (cancelled) return;
        setUser(u);
        setError(null);
        setStatus('authenticated');
      })
      .catch(e => {
        if (cancelled) return;
        if (e instanceof ApiError && e.status === 401) {
          setUser(null);
          setStatus('anonymous');
        } else {
          setError(errorMessage(e));
          setStatus('unavailable');
        }
      });
    return () => {
      cancelled = true;
    };
  }, [attempt]);

  // The server says the session is gone (expired, logged out elsewhere): drop local state.
  useEffect(() => {
    setUnauthorizedHandler(() => {
      setCsrfToken(null);
      clearLocalUserData();
      setUser(null);
      setStatus('anonymous');
    });
    return () => setUnauthorizedHandler(null);
  }, []);

  const adopt = useCallback((u: AuthUser) => {
    clearLocalUserData(); // never inherit another account's cached data
    setUser(u);
    setError(null);
    setStatus('authenticated');
  }, []);

  const value = useMemo<AuthContextType>(
    () => ({
      user,
      status,
      error,
      login: async (email, password) => adopt(await authService.login(email, password)),
      register: async (email, password, displayName) => adopt(await authService.register(email, password, displayName)),
      loginWithGoogle: async credential => adopt(await authService.google(credential)),
      logout: async () => {
        try {
          await authService.logout();
        } catch {
          /* even if the request fails, forget the session locally */
        }
        clearLocalUserData();
        setUser(null);
        setStatus('anonymous');
      },
      retry: () => setAttempt(n => n + 1),
    }),
    [user, status, error, adopt]
  );

  return <AuthContext.Provider value={value}>{children}</AuthContext.Provider>;
};

export function useAuth(): AuthContextType {
  const ctx = useContext(AuthContext);
  if (!ctx) throw new Error('useAuth must be used inside <AuthProvider>');
  return ctx;
}
