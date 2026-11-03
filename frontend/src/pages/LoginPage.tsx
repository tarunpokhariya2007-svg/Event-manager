import React, { useState } from 'react';
import { Link, Navigate, useLocation, useNavigate } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';
import { errorMessage } from '../services/apiClient';
import { AuthLayout, authButtonClass, authInputClass } from '../components/auth/AuthLayout';
import { GoogleSignInButton } from '../components/auth/GoogleSignInButton';

export const LoginPage: React.FC = () => {
  const { status, login, loginWithGoogle } = useAuth();
  const navigate = useNavigate();
  const location = useLocation();
  const from = (location.state as { from?: string } | null)?.from;
  const target = from && from.startsWith('/') && !from.startsWith('//') ? from : '/dashboard';

  const [email, setEmail] = useState('');
  const [password, setPassword] = useState('');
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);

  if (status === 'authenticated') return <Navigate to={target} replace />;

  const run = async (action: () => Promise<void>) => {
    setBusy(true);
    setError(null);
    try {
      await action();
      navigate(target, { replace: true });
    } catch (e) {
      setError(errorMessage(e));
      setBusy(false);
    }
  };

  return (
    <AuthLayout
      title="Welcome back"
      subtitle="Sign in to continue planning your time."
      footer={
        <>
          New to ChronoPlan?{' '}
          <Link to="/signup" className="font-semibold text-white underline underline-offset-4">
            Create an account
          </Link>
        </>
      }
    >
      <form
        onSubmit={e => {
          e.preventDefault();
          void run(() => login(email, password));
        }}
        className="space-y-4"
        noValidate
      >
        <div>
          <label htmlFor="email" className="block text-xs font-medium text-white/80 mb-1.5">
            Email
          </label>
          <input id="email" type="email" autoComplete="email" required value={email} onChange={e => setEmail(e.target.value)} className={authInputClass} placeholder="you@example.com" />
        </div>
        <div>
          <label htmlFor="password" className="block text-xs font-medium text-white/80 mb-1.5">
            Password
          </label>
          <input id="password" type="password" autoComplete="current-password" required value={password} onChange={e => setPassword(e.target.value)} className={authInputClass} placeholder="Your password" />
        </div>
        {error && (
          <p role="alert" className="rounded-lg border border-red-300/40 bg-red-500/20 px-3 py-2 text-sm text-red-100">
            {error}
          </p>
        )}
        <button type="submit" disabled={busy || !email || !password} className={authButtonClass}>
          {busy ? 'Signing in…' : 'Sign in'}
        </button>
      </form>

      <div className="my-5 flex items-center gap-3 text-xs text-white/50">
        <span className="h-px flex-1 bg-white/20" />
        or
        <span className="h-px flex-1 bg-white/20" />
      </div>
      <GoogleSignInButton text="signin_with" onCredential={credential => void run(() => loginWithGoogle(credential))} />
    </AuthLayout>
  );
};
