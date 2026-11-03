import React, { useState, useRef, useEffect } from 'react';
import { useLocation, useNavigate } from 'react-router-dom';
import { useApp } from '../../context/AppContext';
import {
  Search,
  Bell,
  Plus,
  Calendar,
  CheckSquare,
  Sparkles,
  AlertTriangle,
  ChevronDown,
  X
} from 'lucide-react';
import { formatDateReadable, getTodayDateString } from '../../utils/timeUtils';

export const Topbar: React.FC = () => {
  const location = useLocation();
  const navigate = useNavigate();
  const { setActiveModal, conflicts, tasks, settings } = useApp();

  const [showAddMenu, setShowAddMenu] = useState(false);
  const [showNotifications, setShowNotifications] = useState(false);

  const addMenuRef = useRef<HTMLDivElement>(null);
  const notifMenuRef = useRef<HTMLDivElement>(null);

  // Close menus on outside click
  useEffect(() => {
    const handleClickOutside = (e: MouseEvent) => {
      if (addMenuRef.current && !addMenuRef.current.contains(e.target as Node)) {
        setShowAddMenu(false);
      }
      if (notifMenuRef.current && !notifMenuRef.current.contains(e.target as Node)) {
        setShowNotifications(false);
      }
    };
    document.addEventListener('mousedown', handleClickOutside);
    return () => document.removeEventListener('mousedown', handleClickOutside);
  }, []);

  const getPageTitle = (pathname: string): string => {
    switch (pathname) {
      case '/':
      case '/dashboard':
        return 'Overview';
      case '/schedule':
        return 'My Schedule';
      case '/tasks':
        return 'Tasks';
      case '/events':
        return 'Events';
      case '/planner':
        return 'Smart Planner';
      case '/conflicts':
        return 'Conflict Resolution';
      case '/dependencies':
        return 'Task Dependencies';
      case '/analytics':
        return 'Analytics';
      case '/settings':
        return 'Settings';
      default:
        return 'ChronoPlan';
    }
  };

  const todayStr = getTodayDateString();
  const formattedToday = formatDateReadable(todayStr);

  const highPriorityPendingTasks = tasks.filter(
    t => t.priority === 'High' && t.status !== 'Completed'
  );

  const totalNotifications = conflicts.length + highPriorityPendingTasks.length;

  return (
    <header className="h-14 bg-white border-b border-[#DEDEDA] px-6 flex items-center justify-between sticky top-0 z-20">
      {/* Page Title & Breadcrumb Date */}
      <div className="flex items-center gap-3">
        <h2 className="text-base font-semibold text-[#202020] tracking-tight">
          {getPageTitle(location.pathname)}
        </h2>
        <span className="text-[#DEDEDA]">/</span>
        <span className="text-xs text-[#777773] font-normal">
          {formattedToday}
        </span>
      </div>

      {/* Top Navigation Right Controls */}
      <div className="flex items-center gap-3">
        {/* Global Search Button */}
        <button
          onClick={() => setActiveModal('command_palette')}
          className="flex items-center gap-2 px-2.5 py-1.5 bg-[#F7F7F5] hover:bg-[#E8E8E5] text-[#777773] hover:text-[#202020] text-xs rounded-md border border-[#DEDEDA] transition-colors"
        >
          <Search className="w-3.5 h-3.5" />
          <span className="hidden sm:inline">Search or jump to...</span>
          <kbd className="hidden sm:inline-block px-1 py-0.2 text-[10px] bg-[#EFEFED] border border-[#DEDEDA] rounded font-mono text-[#555552]">
            ⌘K
          </kbd>
        </button>

        {/* Notifications Popover */}
        <div className="relative" ref={notifMenuRef}>
          <button
            onClick={() => setShowNotifications(prev => !prev)}
            aria-label="View notifications"
            className="p-2 text-[#555552] hover:text-[#202020] hover:bg-[#F2F2EF] rounded-md transition-colors relative"
          >
            <Bell className="w-4 h-4 stroke-[1.75]" />
            {totalNotifications > 0 && (
              <span className="absolute top-1.5 right-1.5 w-2 h-2 bg-[#92400E] rounded-full" />
            )}
          </button>

          {showNotifications && (
            <div className="absolute right-0 mt-1 w-80 bg-white border border-[#DEDEDA] rounded-md shadow-lg p-3 z-40 text-xs">
              <div className="flex items-center justify-between pb-2 border-b border-[#DEDEDA] mb-2 font-medium text-[#202020]">
                <span>Notifications &amp; Alerts</span>
                <span className="text-[11px] text-[#777773]">
                  {totalNotifications} item{totalNotifications === 1 ? '' : 's'}
                </span>
              </div>

              <div className="space-y-2 max-h-64 overflow-y-auto">
                {conflicts.length > 0 && (
                  <div
                    onClick={() => {
                      setShowNotifications(false);
                      navigate('/conflicts');
                    }}
                    className="p-2 bg-[#FFFBEB] border border-[#FDE68A] rounded cursor-pointer hover:bg-[#FEF3C7] transition-colors"
                  >
                    <div className="flex items-center gap-1.5 font-medium text-[#92400E]">
                      <AlertTriangle className="w-3.5 h-3.5" />
                      <span>{conflicts.length} Overlapping Event Detected</span>
                    </div>
                    <div className="text-[11px] text-[#78350F] mt-1">
                      {conflicts[0].eventA.title} overlaps with {conflicts[0].eventB.title}. Click to review suggestions.
                    </div>
                  </div>
                )}

                {highPriorityPendingTasks.map(task => (
                  <div
                    key={task.id}
                    onClick={() => {
                      setShowNotifications(false);
                      navigate('/tasks');
                    }}
                    className="p-2 bg-[#F7F7F5] border border-[#DEDEDA] rounded cursor-pointer hover:bg-[#E8E8E5] transition-colors"
                  >
                    <div className="font-medium text-[#202020]">{task.title}</div>
                    <div className="text-[11px] text-[#777773] mt-0.5">
                      High Priority • Due {task.deadline.split('T')[1] || 'Today'}
                    </div>
                  </div>
                ))}

                {totalNotifications === 0 && (
                  <div className="py-4 text-center text-[#777773]">
                    No urgent alerts or conflicts.
                  </div>
                )}
              </div>
            </div>
          )}
        </div>

        {/* User Avatar */}
        <div
          onClick={() => navigate('/settings')}
          className="w-7 h-7 rounded-full bg-[#E3E3DF] text-[#202020] flex items-center justify-center font-medium text-xs cursor-pointer border border-[#DEDEDA] hover:border-[#888884] transition-colors"
          title={`Signed in as ${settings.userName}`}
        >
          {settings.userName.charAt(0)}
        </div>

        {/* Add New Button Dropdown */}
        <div className="relative" ref={addMenuRef}>
          <button
            onClick={() => setShowAddMenu(prev => !prev)}
            className="flex items-center gap-1.5 px-3 py-1.5 bg-[#202020] hover:bg-[#333330] text-white text-xs font-medium rounded-md transition-colors"
          >
            <Plus className="w-3.5 h-3.5" />
            <span>Add New</span>
            <ChevronDown className="w-3 h-3 text-[#CCCCCC]" />
          </button>

          {showAddMenu && (
            <div className="absolute right-0 mt-1 w-48 bg-white border border-[#DEDEDA] rounded-md shadow-lg py-1 z-40 text-xs">
              <button
                onClick={() => {
                  setShowAddMenu(false);
                  setActiveModal('add_task');
                }}
                className="w-full flex items-center gap-2 px-3 py-2 text-left text-[#202020] hover:bg-[#E8E8E5] transition-colors"
              >
                <CheckSquare className="w-3.5 h-3.5 text-[#555552]" />
                <span>New Task</span>
              </button>
              <button
                onClick={() => {
                  setShowAddMenu(false);
                  setActiveModal('add_event');
                }}
                className="w-full flex items-center gap-2 px-3 py-2 text-left text-[#202020] hover:bg-[#E8E8E5] transition-colors"
              >
                <Calendar className="w-3.5 h-3.5 text-[#555552]" />
                <span>New Event</span>
              </button>
              <div className="border-t border-[#DEDEDA] my-1" />
              <button
                onClick={() => {
                  setShowAddMenu(false);
                  navigate('/planner');
                }}
                className="w-full flex items-center gap-2 px-3 py-2 text-left text-[#202020] hover:bg-[#E8E8E5] transition-colors"
              >
                <Sparkles className="w-3.5 h-3.5 text-[#555552]" />
                <span>Run Smart Planner</span>
              </button>
            </div>
          )}
        </div>
      </div>
    </header>
  );
};
