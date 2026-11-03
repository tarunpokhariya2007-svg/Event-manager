import React, { useEffect, useRef, useState } from 'react';
import { GOOGLE_CLIENT_ID } from '../../config/api';

interface GoogleCredentialResponse {
  credential?: string;
}

interface GoogleIdApi {
  initialize: (config: { client_id: string; callback: (r: GoogleCredentialResponse) => void }) => void;
  renderButton: (el: HTMLElement, options: Record<string, unknown>) => void;
}

declare global {
  interface Window {
    google?: { accounts: { id: GoogleIdApi } };
  }
}

const SCRIPT_SRC = 'https://accounts.google.com/gsi/client';
let scriptPromise: Promise<void> | null = null;

function loadGoogleScript(): Promise<void> {
  if (window.google?.accounts?.id) return Promise.resolve();
  if (!scriptPromise) {
    scriptPromise = new Promise<void>((resolve, reject) => {
      const el = document.createElement('script');
      el.src = SCRIPT_SRC;
      el.async = true;
      el.defer = true;
      el.onload = () => resolve();
      el.onerror = () => {
        scriptPromise = null; // allow a retry on the next mount
        reject(new Error('Could not load Google sign-in.'));
      };
      document.head.appendChild(el);
    });
  }
  return scriptPromise;
}

interface Props {
  /** Receives the Google ID token. It is only forwarded to our backend, which verifies it. */
  onCredential: (credential: string) => void;
  text?: 'signin_with' | 'signup_with' | 'continue_with';
}

/** Google Identity Services button. Shows a disabled placeholder when no client id is configured. */
export const GoogleSignInButton: React.FC<Props> = ({ onCredential, text = 'continue_with' }) => {
  const holder = useRef<HTMLDivElement>(null);
  const callback = useRef(onCredential);
  callback.current = onCredential;
  const [failed, setFailed] = useState(false);

  useEffect(() => {
    if (!GOOGLE_CLIENT_ID) return;
    let cancelled = false;
    loadGoogleScript()
      .then(() => {
        if (cancelled || !holder.current || !window.google) return;
        window.google.accounts.id.initialize({
          client_id: GOOGLE_CLIENT_ID,
          callback: r => {
            if (r.credential) callback.current(r.credential);
          },
        });
        window.google.accounts.id.renderButton(holder.current, {
          type: 'standard',
          theme: 'outline',
          size: 'large',
          shape: 'pill',
          text,
          width: Math.min(360, holder.current.clientWidth || 360),
        });
      })
      .catch(() => !cancelled && setFailed(true));
    return () => {
      cancelled = true;
    };
  }, [text]);

  if (!GOOGLE_CLIENT_ID || failed) {
    return (
      <button
        type="button"
        disabled
        title={failed ? 'Google sign-in could not be loaded' : 'Set VITE_GOOGLE_CLIENT_ID to enable Google sign-in'}
        className="w-full py-2.5 rounded-full border border-white/20 bg-white/5 text-white/50 text-sm cursor-not-allowed"
      >
        {failed ? 'Google sign-in unavailable' : 'Google sign-in not configured'}
      </button>
    );
  }
  return <div ref={holder} className="w-full flex justify-center min-h-[44px]" />;
};
