import React from 'react';
import { PageHeader } from '../components/common/PageHeader';
import { CalendarView } from '../components/schedule/CalendarView';
import { useApp } from '../context/AppContext';
import { Plus, CalendarDays } from 'lucide-react';

export const SchedulePage: React.FC = () => {
  const { setActiveModal, setSelectedEvent } = useApp();

  return (
    <div className="space-y-6">
      <PageHeader
        heading="My Schedule"
        description="Comprehensive calendar timeline with fixed commitments and automated task allocations."
      >
        <button
          onClick={() => {
            setSelectedEvent(null);
            setActiveModal('add_event');
          }}
          className="inline-flex items-center gap-1.5 px-3 py-1.5 bg-[#202020] hover:bg-[#333330] text-white text-xs font-medium rounded-md transition-colors"
        >
          <Plus className="w-3.5 h-3.5" />
          <span>New Event</span>
        </button>
      </PageHeader>

      <CalendarView />
    </div>
  );
};
