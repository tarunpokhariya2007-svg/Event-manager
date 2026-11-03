import React from 'react';
import { BrowserRouter, Routes, Route, Navigate, Outlet } from 'react-router-dom';
import { AppProvider } from './context/AppContext';
import { AuthProvider, useAuth } from './context/AuthContext';
import { ProtectedRoute } from './components/auth/ProtectedRoute';
import { LandingPage } from './pages/LandingPage';
import { LoginPage } from './pages/LoginPage';
import { SignupPage } from './pages/SignupPage';
import { DashboardLayout } from './components/layout/DashboardLayout';
import { OverviewPage } from './pages/OverviewPage';
import { SchedulePage } from './pages/SchedulePage';
import { TasksPage } from './pages/TasksPage';
import { EventsPage } from './pages/EventsPage';
import { SmartPlannerPage } from './pages/SmartPlannerPage';
import { ConflictsPage } from './pages/ConflictsPage';
import { DependenciesPage } from './pages/DependenciesPage';
import { AnalyticsPage } from './pages/AnalyticsPage';
import { SettingsPage } from './pages/SettingsPage';

/**
 * Application data (tasks, events, settings) is loaded only after sign-in and is re-created
 * from scratch whenever the account changes (key), so one user's data never lingers for another.
 */
function UserScopedApp() {
  const { user } = useAuth();
  return (
    <AppProvider key={user?.id}>
      <Outlet />
    </AppProvider>
  );
}

export function App() {
  return (
    <AuthProvider>
      <BrowserRouter>
        <Routes>
          {/* Public */}
          <Route path="/" element={<LandingPage />} />
          <Route path="/login" element={<LoginPage />} />
          <Route path="/signup" element={<SignupPage />} />

          {/* Protected: the existing dashboard and its pages, unchanged */}
          <Route element={<ProtectedRoute />}>
            <Route element={<UserScopedApp />}>
              <Route element={<DashboardLayout />}>
                <Route path="dashboard" element={<OverviewPage />} />
                <Route path="schedule" element={<SchedulePage />} />
                <Route path="tasks" element={<TasksPage />} />
                <Route path="events" element={<EventsPage />} />
                <Route path="planner" element={<SmartPlannerPage />} />
                <Route path="conflicts" element={<ConflictsPage />} />
                <Route path="dependencies" element={<DependenciesPage />} />
                <Route path="analytics" element={<AnalyticsPage />} />
                <Route path="settings" element={<SettingsPage />} />
              </Route>
            </Route>
          </Route>

          <Route path="*" element={<Navigate to="/" replace />} />
        </Routes>
      </BrowserRouter>
    </AuthProvider>
  );
}

export default App;
