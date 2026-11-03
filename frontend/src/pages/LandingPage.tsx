import React from 'react';
import { Link } from 'react-router-dom';
import { BarChart3, CalendarClock, GitBranch, ShieldCheck, Sparkles, TriangleAlert } from 'lucide-react';
import { useAuth } from '../context/AuthContext';

const features = [
  { icon: Sparkles, title: 'Smart scheduling', text: 'A C++ engine places your tasks around fixed events, deadlines and priorities in milliseconds.' },
  { icon: GitBranch, title: 'Task dependencies', text: 'Model what must happen first. Cycles are caught before they can break your plan.' },
  { icon: TriangleAlert, title: 'Conflict detection', text: 'Overlaps and at-risk deadlines surface early, with alternative slots suggested.' },
  { icon: BarChart3, title: 'Clear analytics', text: 'See workload, focus time and completion trends at a glance.' },
  { icon: ShieldCheck, title: 'Private by design', text: 'Your tasks and events belong to your account only, protected by secure sessions.' },
];

export const LandingPage: React.FC = () => {
  const { status } = useAuth();
  const signedIn = status === 'authenticated';

  return (
    <div className="relative min-h-screen overflow-hidden bg-gradient-to-br from-[#0f0c29] via-[#302b63] to-[#24243e] text-white">
      <div className="pointer-events-none absolute -top-40 left-1/4 w-[32rem] h-[32rem] rounded-full bg-violet-500/30 blur-3xl" />
      <div className="pointer-events-none absolute top-1/3 -right-40 w-[34rem] h-[34rem] rounded-full bg-blue-500/25 blur-3xl" />

      <header className="relative max-w-6xl mx-auto px-5 py-5 flex items-center justify-between">
        <Link to="/" className="flex items-center gap-2">
          <span className="w-9 h-9 rounded-xl bg-gradient-to-br from-violet-500 to-blue-500 flex items-center justify-center shadow-lg">
            <CalendarClock className="w-5 h-5" />
          </span>
          <span className="text-lg font-semibold tracking-tight">ChronoPlan</span>
        </Link>
        <nav className="flex items-center gap-2 text-sm">
          {signedIn ? (
            <Link to="/dashboard" className="rounded-full bg-white px-4 py-2 font-semibold text-[#302b63] hover:bg-white/90">
              Open dashboard
            </Link>
          ) : (
            <>
              <Link to="/login" className="rounded-full px-4 py-2 text-white/80 hover:text-white">
                Log in
              </Link>
              <Link to="/signup" className="rounded-full bg-white px-4 py-2 font-semibold text-[#302b63] hover:bg-white/90">
                Get started
              </Link>
            </>
          )}
        </nav>
      </header>

      <main className="relative max-w-6xl mx-auto px-5">
        <section className="pt-14 pb-16 sm:pt-24 sm:pb-24 text-center max-w-3xl mx-auto">
          <span className="inline-flex items-center gap-2 rounded-full border border-white/20 bg-white/10 backdrop-blur px-3 py-1 text-xs text-white/80">
            <Sparkles className="w-3.5 h-3.5" /> Intelligent scheduling, powered by C++
          </span>
          <h1 className="mt-6 text-4xl sm:text-6xl font-semibold tracking-tight leading-[1.05]">
            Plan your time with{' '}
            <span className="bg-gradient-to-r from-violet-300 via-fuchsia-300 to-sky-300 bg-clip-text text-transparent">precision</span>
          </h1>
          <p className="mt-5 text-base sm:text-lg text-white/70">
            ChronoPlan turns tasks, deadlines and meetings into a realistic schedule, then keeps it healthy as your week changes.
          </p>
          <div className="mt-8 flex flex-col sm:flex-row items-center justify-center gap-3">
            <Link
              to={signedIn ? '/dashboard' : '/signup'}
              className="w-full sm:w-auto rounded-full bg-gradient-to-r from-violet-500 to-blue-500 px-7 py-3 text-sm font-semibold shadow-lg shadow-violet-900/40 hover:brightness-110"
            >
              {signedIn ? 'Go to your dashboard' : 'Start planning free'}
            </Link>
            {!signedIn && (
              <Link to="/login" className="w-full sm:w-auto rounded-full border border-white/25 bg-white/10 backdrop-blur px-7 py-3 text-sm font-semibold hover:bg-white/15">
                I already have an account
              </Link>
            )}
          </div>
        </section>

        <section className="pb-20 grid gap-4 sm:grid-cols-2 lg:grid-cols-3">
          {features.map(f => (
            <div key={f.title} className="rounded-2xl border border-white/15 bg-white/10 backdrop-blur-xl p-5 shadow-xl">
              <span className="w-10 h-10 rounded-xl bg-gradient-to-br from-violet-500/80 to-blue-500/80 flex items-center justify-center">
                <f.icon className="w-5 h-5" />
              </span>
              <h2 className="mt-4 text-base font-semibold">{f.title}</h2>
              <p className="mt-1.5 text-sm text-white/70 leading-relaxed">{f.text}</p>
            </div>
          ))}
        </section>
      </main>

      <footer className="relative border-t border-white/10 py-6 text-center text-xs text-white/50">
        © {new Date().getFullYear()} ChronoPlan
      </footer>
    </div>
  );
};
