import React, { useState } from 'react';
import { Link, Navigate, useNavigate } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';
import { errorMessage } from '../services/apiClient';
import { AuthLayout, authButtonClass, authInputClass } from '../components/auth/AuthLayout';
import { GoogleSignInButton } from '../components/auth/GoogleSignInButton';

export const SignupPage: React.FC = () => {
  const { status, register, loginWithGoogle } = useAuth();
  const navigate = useNavigate();

  const [name, setName] = useState('');
  const [email, setEmail] = useState('');
  const [password, setPassword] = useState('');
  const [confirm, setConfirm] = useState('');
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);

  if (status === 'authenticated') return <Navigate to="/dashboard" replace />;

  const run = async (action: () => Promise<void>) => {
    setBusy(true);
    setError(null);
    try {
      await action();
      navigate('/dashboard', { replace: true });
    } catch (e) {
      setError(errorMessage(e));
      setBusy(false);
    }
  };

  const mismatch = confirm.length > 0 && confirm !== password;

  return (
    <AuthLayout
      title="Create your account"
      subtitle="Start scheduling smarter in under a minute."
      footer={
        <>
          Already have an account?{' '}
          <Link to="/login" className="font-semibold text-white underline underline-offset-4">
            Sign in
          </Link>
        </>
      }
    >
      <form
        onSubmit={e => {
          e.preventDefault();
          if (mismatch) return;
          void run(() => register(email, password, name));
        }}
        className="space-y-4"
        noValidate
      >
        <div>
          <label htmlFor="name" className="block text-xs font-medium text-white/80 mb-1.5">
            Name <span className="text-white/40">(optional)</span>
          </label>
          <input id="name" type="text" autoComplete="name" maxLength={100} value={name} onChange={e => setName(e.target.value)} className={authInputClass} placeholder="Ada Lovelace" />
        </div>
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
          <input id="password" type="password" autoComplete="new-password" required minLength={8} maxLength={128} value={password} onChange={e => setPassword(e.target.value)} className={authInputClass} placeholder="At least 8 characters" />
        </div>
        <div>
          <label htmlFor="confirm" className="block text-xs font-medium text-white/80 mb-1.5">
            Confirm password
          </label>
          <input id="confirm" type="password" autoComplete="new-password" required value={confirm} onChange={e => setConfirm(e.target.value)} className={authInputClass} placeholder="Repeat your password" />
          {mismatch && <p className="mt-1 text-xs text-red-200">Passwords do not match.</p>}
        </div>
        {error && (
          <p role="alert" className="rounded-lg border border-red-300/40 bg-red-500/20 px-3 py-2 text-sm text-red-100">
            {error}
          </p>
        )}
        <button type="submit" disabled={busy || !email || password.length < 8 || mismatch || !confirm} className={authButtonClass}>
          {busy ? 'Creating account…' : 'Create account'}
        </button>
      </form>

      <div className="my-5 flex items-center gap-3 text-xs text-white/50">
        <span className="h-px flex-1 bg-white/20" />
        or
        <span className="h-px flex-1 bg-white/20" />
      </div>
      <GoogleSignInButton text="signup_with" onCredential={credential => void run(() => loginWithGoogle(credential))} />
    </AuthLayout>
  );
};
