import React from 'react';
import { Outlet } from 'react-router-dom';
import { Sidebar } from './Sidebar';
import { Topbar } from './Topbar';
import { ToastContainer } from '../common/ToastContainer';
import { CommandPalette } from '../common/CommandPalette';
import { TaskModal } from '../tasks/TaskModal';
import { EventModal } from '../events/EventModal';
import { EventDetailsModal } from '../schedule/EventDetailsModal';
import { useApp } from '../../context/AppContext';

export const DashboardLayout: React.FC = () => {
  const { backendStatus, backendError, reloadData } = useApp();
  return (
    <div className="min-h-screen bg-[#F7F7F5] flex">
      {/* Fixed Left Sidebar (240px) */}
      <Sidebar />

      {/* Main Content Area */}
      <div className="flex-1 ml-60 flex flex-col min-w-0">
        <Topbar />
        {backendStatus === 'offline' && (
          <div className="mx-6 md:mx-8 mt-4 px-4 py-2.5 bg-[#FEF2F2] border border-[#FECACA] rounded-md text-xs text-[#991B1B] flex items-center justify-between gap-3">
            <span>C++ backend unavailable: {backendError} Tasks and events cannot be loaded or saved.</span>
            <button onClick={reloadData} className="font-medium underline flex-shrink-0">Retry</button>
          </div>
        )}

        <main className="flex-1 p-6 md:p-8 max-w-7xl w-full mx-auto">
          <Outlet />
        </main>
      </div>

      {/* Floating System Components & Modals */}
      <ToastContainer />
      <CommandPalette />
      <TaskModal />
      <EventModal />
      <EventDetailsModal />
    </div>
  );
};
