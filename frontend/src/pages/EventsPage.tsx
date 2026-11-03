import React from 'react';
import { PageHeader } from '../components/common/PageHeader';
import { EventTable } from '../components/events/EventTable';
import { useApp } from '../context/AppContext';
import { Plus } from 'lucide-react';

export const EventsPage: React.FC = () => {
  const { setActiveModal, setSelectedEvent } = useApp();

  return (
    <div className="space-y-6">
      <PageHeader
        heading="Events"
        description="Schedule and coordinate lectures, lab sessions, meetings, and milestones."
      >
        <button
          onClick={() => {
            setSelectedEvent(null);
            setActiveModal('add_event');
          }}
          className="inline-flex items-center gap-1.5 px-3 py-1.5 bg-[#202020] hover:bg-[#333330] text-white text-xs font-medium rounded-md transition-colors"
        >
          <Plus className="w-3.5 h-3.5" />
          <span>Add Event</span>
        </button>
      </PageHeader>

      <EventTable />
    </div>
  );
};
