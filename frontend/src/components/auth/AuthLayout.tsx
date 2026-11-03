import React from 'react';
import { Link } from 'react-router-dom';
import { CalendarClock } from 'lucide-react';

/** Gradient + glass card shared by the login and sign-up pages. */
export const AuthLayout: React.FC<{ title: string; subtitle: string; children: React.ReactNode; footer: React.ReactNode }> = ({
  title,
  subtitle,
  children,
  footer,
}) => (
  <div className="relative min-h-screen overflow-hidden bg-gradient-to-br from-[#0f0c29] via-[#302b63] to-[#24243e] flex items-center justify-center p-4">
    <div className="pointer-events-none absolute -top-32 -left-24 w-96 h-96 rounded-full bg-violet-500/30 blur-3xl" />
    <div className="pointer-events-none absolute -bottom-32 -right-24 w-[28rem] h-[28rem] rounded-full bg-blue-500/30 blur-3xl" />

    <div className="relative w-full max-w-md">
      <Link to="/" className="flex items-center justify-center gap-2 mb-6 text-white">
        <span className="w-9 h-9 rounded-xl bg-gradient-to-br from-violet-500 to-blue-500 flex items-center justify-center shadow-lg">
          <CalendarClock className="w-5 h-5" />
        </span>
        <span className="text-xl font-semibold tracking-tight">ChronoPlan</span>
      </Link>

      <div className="rounded-3xl border border-white/15 bg-white/10 backdrop-blur-xl shadow-2xl p-6 sm:p-8 text-white">
        <h1 className="text-2xl font-semibold tracking-tight">{title}</h1>
        <p className="text-sm text-white/70 mt-1 mb-6">{subtitle}</p>
        {children}
      </div>
      <p className="text-center text-sm text-white/70 mt-5">{footer}</p>
    </div>
  </div>
);

export const authInputClass =
  'w-full rounded-xl border border-white/20 bg-white/10 px-3.5 py-2.5 text-sm text-white placeholder-white/40 outline-none focus:border-violet-300 focus:ring-2 focus:ring-violet-400/40';
export const authButtonClass =
  'w-full rounded-xl bg-gradient-to-r from-violet-500 to-blue-500 py-2.5 text-sm font-semibold text-white shadow-lg shadow-violet-900/30 transition hover:brightness-110 disabled:opacity-60 disabled:cursor-not-allowed';
