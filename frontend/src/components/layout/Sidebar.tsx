import React from 'react';
import { NavLink, useNavigate } from 'react-router-dom';
import { useAuth } from '../../context/AuthContext';
import { useApp } from '../../context/AppContext';
import {
  LayoutDashboard,
  Calendar,
  CheckSquare,
  CalendarDays,
  Sparkles,
  GitBranch,
  BarChart3,
  Settings,
  AlertTriangle,
  User,
  LogOut,
  Sliders
} from 'lucide-react';

export const Sidebar: React.FC = () => {
  const { conflicts, settings, resetToDefaults } = useApp();
  const navigate = useNavigate();
  const { logout } = useAuth();

  const navLinks = [
    { name: 'Overview', path: '/dashboard', icon: LayoutDashboard },
    { name: 'My Schedule', path: '/schedule', icon: Calendar },
    { name: 'Tasks', path: '/tasks', icon: CheckSquare },
    { name: 'Events', path: '/events', icon: CalendarDays },
    {
      name: 'Smart Planner',
      path: '/planner',
      icon: Sparkles,
      badge: 'Core'
    },
    {
      name: 'Conflicts',
      path: '/conflicts',
      icon: AlertTriangle,
      badgeCount: conflicts.length
    },
    { name: 'Dependencies', path: '/dependencies', icon: GitBranch },
    { name: 'Analytics', path: '/analytics', icon: BarChart3 },
    { name: 'Settings', path: '/settings', icon: Settings },
  ];

  return (
    <aside className="w-60 min-w-[240px] max-w-[240px] h-screen bg-[#EFEFED] border-r border-[#DEDEDA] flex flex-col justify-between select-none fixed left-0 top-0 z-30">
      {/* Brand & Wordmark */}
      <div className="flex flex-col">
        <div className="h-14 px-4 flex items-center gap-2.5 border-b border-[#DEDEDA]">
          <div className="w-7 h-7 bg-[#202020] text-white rounded flex items-center justify-center font-bold text-sm tracking-tighter">
            CP
          </div>
          <div className="flex flex-col">
            <span className="font-semibold text-sm tracking-tight text-[#202020]">
              ChronoPlan
            </span>
            <span className="text-[10px] text-[#777773] tracking-wide uppercase font-medium">
              Intelligent Scheduler
            </span>
          </div>
        </div>

        {/* Navigation Items */}
        <nav className="p-2 space-y-0.5 mt-2">
          {navLinks.map(link => (
            <NavLink
              key={link.path}
              to={link.path}
              end={link.path === '/'}
              className={({ isActive }) =>
                `flex items-center justify-between px-2.5 py-1.5 rounded-md text-xs font-medium transition-colors ${
                  isActive
                    ? 'bg-[#E3E3DF] text-[#202020] font-semibold'
                    : 'text-[#4A4A46] hover:bg-[#E8E8E5] hover:text-[#202020]'
                }`
              }
            >
              <div className="flex items-center gap-2.5">
                <link.icon className="w-4 h-4 text-[#555552] stroke-[1.75]" />
                <span>{link.name}</span>
              </div>
              {link.badge && (
                <span className="px-1.5 py-0.5 text-[10px] uppercase font-semibold tracking-wider bg-[#202020] text-white rounded">
                  {link.badge}
                </span>
              )}
              {link.badgeCount !== undefined && link.badgeCount > 0 && (
                <span className="px-1.5 py-0.2 text-[10px] font-semibold bg-[#FFFBEB] text-[#92400E] border border-[#FDE68A] rounded-full">
                  {link.badgeCount}
                </span>
              )}
            </NavLink>
          ))}
        </nav>
      </div>

      {/* User Profile & Footer Actions */}
      <div className="p-3 border-t border-[#DEDEDA] bg-[#EFEFED]">
        <div className="flex items-center justify-between mb-2 px-1">
          <div className="flex items-center gap-2">
            <div className="w-7 h-7 rounded-full bg-[#DEDEDA] text-[#202020] flex items-center justify-center font-medium text-xs">
              {settings.userName.charAt(0)}
            </div>
            <div className="flex flex-col min-w-0">
              <span className="text-xs font-semibold text-[#202020] truncate">
                {settings.userName}
              </span>
              <span className="text-[10px] text-[#777773] truncate max-w-[120px]">
                {settings.userRole}
              </span>
            </div>
          </div>
          <button
            onClick={() => navigate('/settings')}
            title="Account Settings"
            className="p-1 text-[#777773] hover:text-[#202020] hover:bg-[#E8E8E5] rounded transition-colors"
          >
            <Sliders className="w-3.5 h-3.5" />
          </button>
        </div>

        <div className="flex items-center justify-between text-[11px] text-[#777773] px-1 pt-1 border-t border-[#E5E5E2]">
          <button
            onClick={() => navigate('/settings')}
            className="hover:text-[#202020] flex items-center gap-1 transition-colors"
          >
            <User className="w-3 h-3" />
            <span>Profile</span>
          </button>
          <button
            onClick={resetToDefaults}
            title="Reset to sample dataset"
            className="hover:text-[#202020] transition-colors"
          >
            Reset Demo
          </button>
          <button
            onClick={async () => {
              await logout();
              navigate('/', { replace: true });
            }}
            className="hover:text-[#202020] flex items-center gap-1 transition-colors"
          >
            <LogOut className="w-3 h-3" />
            <span>Logout</span>
          </button>
        </div>
      </div>
    </aside>
  );
};
