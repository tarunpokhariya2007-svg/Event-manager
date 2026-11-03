import React from 'react';
import { Navigate, Outlet, useLocation } from 'react-router-dom';
import { useAuth } from '../../context/AuthContext';

/** Renders nested routes only for a signed-in user; everyone else is sent to /login. */
export const ProtectedRoute: React.FC = () => {
  const { status, error, retry } = useAuth();
  const location = useLocation();

  if (status === 'loading') {
    return (
      <div className="min-h-screen flex items-center justify-center bg-[#0f0c29] text-white/80 text-sm">
        <div className="flex items-center gap-3">
          <span className="w-4 h-4 rounded-full border-2 border-white/30 border-t-white animate-spin" />
          Restoring your session…
        </div>
      </div>
    );
  }

  if (status === 'unavailable') {
    return (
      <div className="min-h-screen flex items-center justify-center bg-[#0f0c29] p-6">
        <div className="max-w-sm text-center rounded-2xl border border-white/15 bg-white/10 backdrop-blur-xl p-6 text-white">
          <h1 className="text-base font-semibold mb-2">Can’t reach ChronoPlan</h1>
          <p className="text-sm text-white/70 mb-4">{error}</p>
          <button onClick={retry} className="px-4 py-2 rounded-lg bg-white text-[#302b63] text-sm font-semibold">
            Try again
          </button>
        </div>
      </div>
    );
  }

  if (status !== 'authenticated') {
    return <Navigate to="/login" replace state={{ from: location.pathname + location.search }} />;
  }
  return <Outlet />;
};
