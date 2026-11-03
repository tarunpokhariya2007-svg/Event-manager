import React, { useMemo } from 'react';
import { PageHeader } from '../components/common/PageHeader';
import { ConflictCard } from '../components/conflicts/ConflictCard';
import { useApp } from '../context/AppContext';
import { CheckCircle2 } from 'lucide-react';
import { classifyConflict } from '../utils/conflictUtils';
import { useNavigate } from 'react-router-dom';

export const ConflictsPage: React.FC = () => {
  const { conflicts } = useApp();
  const navigate = useNavigate();
  const severityCounts = useMemo(() => {
    const c = { high: 0, medium: 0, low: 0 };
    conflicts.forEach(x => { c[classifyConflict(x).severity] += 1; });
    return c;
  }, [conflicts]);

  return (
    <div className="space-y-6">
      <PageHeader
        heading="Conflict Resolution"
        description="Inspect overlapping calendar commitments and apply optimized conflict-free alternative time slots."
      >
        <div className="flex items-center gap-2 text-xs font-mono px-3 py-1.5 rounded-md border bg-white border-[#DEDEDA]">
          <span className="text-[#777773]">Active Conflicts:</span>
          <span className={`font-bold ${conflicts.length > 0 ? 'text-[#92400E]' : 'text-[#166534]'}`}>
            {conflicts.length}
          </span>
        </div>
      </PageHeader>

      {conflicts.length === 0 ? (
        <div className="bg-white border border-[#DEDEDA] rounded-lg p-12 text-center">
          <div className="w-12 h-12 rounded-full bg-[#F0FDF4] text-[#166534] border border-[#BBF7D0] flex items-center justify-center mx-auto mb-3">
            <CheckCircle2 className="w-6 h-6 stroke-[1.75]" />
          </div>
          <h3 className="text-base font-semibold text-[#202020] mb-1">
            Zero Scheduling Conflicts Detected
          </h3>
          <p className="text-xs text-[#777773] max-w-md mx-auto leading-relaxed mb-4">
            All calendar events have mutually exclusive time allocations. The C++ scheduling engine has unobstructed windows for placing flexible tasks.
          </p>
          <button
            onClick={() => navigate('/schedule')}
            className="px-4 py-2 text-xs font-medium bg-[#202020] text-white rounded-md hover:bg-[#333330] transition-colors"
          >
            Return to My Schedule
          </button>
        </div>
      ) : (
        <div className="space-y-4">
          <div className="p-3.5 bg-[#FFFBEB] border border-[#FDE68A] rounded-md text-xs text-[#92400E]">
            <span className="font-semibold">{severityCounts.high} hard clash{severityCounts.high === 1 ? '' : 'es'}, {severityCounts.medium} major, {severityCounts.low} minor.</span> Hard clashes involve two Fixed events and need a manual edit. For the others, pick a free gap below to move the flexible event. Hover a label to see how it was classified.
          </div>

          <div className="space-y-6">
            {conflicts.map(conflict => (
              <ConflictCard key={conflict.id} conflict={conflict} />
            ))}
          </div>
        </div>
      )}
    </div>
  );
};
