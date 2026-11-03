import React from 'react';
import { History, Trash2, Eye } from 'lucide-react';
import { ScheduleHistoryEntry } from '../../types/planner';
import { formatDateReadable } from '../../utils/timeUtils';

interface Props {
  entries: ScheduleHistoryEntry[];
  activeId: string | null;
  onSelect: (id: string) => void;
  onDelete: (id: string) => void;
  onClear: () => void;
}

const STATUS_STYLE: Record<string, string> = {
  success: 'bg-[#F0FDF4] text-[#166534] border-[#BBF7D0]',
  partial: 'bg-[#FFFBEB] text-[#92400E] border-[#FDE68A]',
  infeasible: 'bg-[#FEF2F2] text-[#991B1B] border-[#FECACA]',
};

export const ScheduleHistoryPanel: React.FC<Props> = ({ entries, activeId, onSelect, onDelete, onClear }) => (
  <div className="bg-white border border-[#DEDEDA] rounded-lg p-4">
    <div className="flex items-center justify-between pb-2.5 border-b border-[#DEDEDA] mb-3">
      <div className="flex items-center gap-1.5 text-xs font-semibold text-[#202020]">
        <History className="w-3.5 h-3.5 text-[#555552]" />
        <span className="uppercase tracking-wider text-[11px]">Schedule history ({entries.length})</span>
      </div>
      {entries.length > 0 && (
        <button onClick={onClear} className="text-[11px] text-[#777773] hover:text-[#991B1B] transition-colors">
          Clear all
        </button>
      )}
    </div>

    {entries.length === 0 ? (
      <p className="text-[11px] text-[#777773] leading-relaxed">
        Each schedule you generate is saved here so you can look back at it. History is kept in this browser only; the backend does not store generated schedules.
      </p>
    ) : (
      <ul className="space-y-1.5 max-h-56 overflow-y-auto">
        {entries.map(entry => {
          const r = entry.result;
          const active = entry.id === activeId;
          return (
            <li
              key={entry.id}
              className={`flex items-center justify-between gap-2 p-2.5 rounded-md border text-xs ${active ? 'border-[#202020] bg-[#F7F7F5]' : 'border-[#E8E8E5] bg-white'}`}
            >
              <div className="min-w-0">
                <div className="flex items-center gap-2 flex-wrap">
                  <span className="font-medium text-[#202020]">{formatDateReadable(r.targetDate)}</span>
                  <span className={`px-1.5 rounded border text-[10px] font-semibold ${STATUS_STYLE[r.status] ?? ''}`}>{r.status}</span>
                </div>
                <div className="text-[11px] text-[#777773] truncate">
                  Generated {new Date(entry.savedAt).toLocaleString([], { dateStyle: 'medium', timeStyle: 'short' })} • {r.allocatedTasksCount} placed, {r.unscheduledTasks.length} unscheduled • {r.objective}
                </div>
              </div>
              <div className="flex items-center gap-1 flex-shrink-0">
                <button
                  onClick={() => onSelect(entry.id)}
                  title="Inspect this past schedule (read-only)"
                  className="inline-flex items-center gap-1 px-2 py-1 rounded border border-[#DEDEDA] bg-[#F7F7F5] hover:bg-[#E8E8E5] text-[11px]"
                >
                  <Eye className="w-3 h-3" />
                  {active ? 'Viewing' : 'View'}
                </button>
                <button onClick={() => onDelete(entry.id)} title="Remove from history" className="p-1 text-[#777773] hover:text-[#991B1B]">
                  <Trash2 className="w-3.5 h-3.5" />
                </button>
              </div>
            </li>
          );
        })}
      </ul>
    )}
  </div>
);
