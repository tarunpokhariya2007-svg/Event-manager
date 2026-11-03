import React, { useEffect, useState } from 'react';
import { Link } from 'react-router-dom';
import { PageHeader } from '../components/common/PageHeader';
import { PlannerConfigPanel } from '../components/planner/PlannerConfigPanel';
import { PlannerResultsView } from '../components/planner/PlannerResultsView';
import { ScheduleHistoryPanel } from '../components/planner/ScheduleHistoryPanel';
import { useApp } from '../context/AppContext';
import { Terminal } from 'lucide-react';

export const SmartPlannerPage: React.FC = () => {
  const { schedulingResult, scheduleHistory, deleteHistoryEntry, clearScheduleHistory, conflicts, tasks, events } = useApp();
  const [previewId, setPreviewId] = useState<string | null>(null);
  const previewEntry = scheduleHistory.find(e => e.id === previewId) ?? null;

  // A freshly generated schedule always replaces any historical preview.
  useEffect(() => setPreviewId(null), [schedulingResult]);

  const quickLinks = [
    { to: '/tasks', label: `Tasks (${tasks.length})` },
    { to: '/events', label: `Events (${events.length})` },
    { to: '/dependencies', label: 'Dependencies' },
    { to: '/conflicts', label: `Conflicts (${conflicts.length})`, warn: conflicts.length > 0 },
  ];

  return (
    <div className="space-y-6">
      <PageHeader heading="Smart Planner" description="Turn your commitments into a workable schedule.">
        <div className="flex items-center gap-2 text-xs text-[#777773] bg-[#EFEFED] px-3 py-1.5 rounded-md border border-[#DEDEDA]">
          <Terminal className="w-3.5 h-3.5 text-[#555552]" />
          <span>C++ Constraint Solver Protocol v2</span>
        </div>
      </PageHeader>

      <div className="flex flex-wrap gap-2 text-[11px]">
        {quickLinks.map(l => (
          <Link
            key={l.to}
            to={l.to}
            className={`px-2.5 py-1 rounded-md border transition-colors ${l.warn ? 'bg-[#FFFBEB] border-[#FDE68A] text-[#92400E]' : 'bg-white border-[#DEDEDA] text-[#555552] hover:bg-[#F7F7F5]'}`}
          >
            {l.label}
          </Link>
        ))}
      </div>

      <PlannerConfigPanel />

      <ScheduleHistoryPanel
        entries={scheduleHistory}
        activeId={previewId}
        onSelect={setPreviewId}
        onDelete={deleteHistoryEntry}
        onClear={clearScheduleHistory}
      />

      <PlannerResultsView key={previewId ?? 'current'} historicalEntry={previewEntry} onExitHistory={() => setPreviewId(null)} />
    </div>
  );
};
