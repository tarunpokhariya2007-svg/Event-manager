import React from 'react';
import { ScheduledTimelineItem } from '../../types/planner';
import { formatTime12h, formatDuration } from '../../utils/timeUtils';
import { StatusBadge } from '../common/StatusBadge';
import { PriorityBadge } from '../common/PriorityBadge';
import { Calendar, CheckSquare, Coffee, ChevronRight, Clock } from 'lucide-react';

interface ScheduleTimelineProps {
  items: ScheduledTimelineItem[];
  showCurrentTimeIndicator?: boolean;
  onItemClick?: (item: ScheduledTimelineItem) => void;
}

export const ScheduleTimeline: React.FC<ScheduleTimelineProps> = ({
  items,
  showCurrentTimeIndicator = true,
  onItemClick
}) => {
  // Current time formatted for indicator
  const now = new Date();
  const currentHours = now.getHours().toString().padStart(2, '0');
  const currentMinutes = now.getMinutes().toString().padStart(2, '0');
  const currentTimeString = `${currentHours}:${currentMinutes}`;
  const currentTimeFormatted = formatTime12h(currentTimeString);

  return (
    <div className="relative border-l border-[#DEDEDA] ml-4 pl-6 space-y-6">
      {/* Current Time Indicator line (simulated/active) */}
      {showCurrentTimeIndicator && (
        <div className="absolute left-[-5px] top-28 z-10 flex items-center pointer-events-none">
          <div className="w-2.5 h-2.5 rounded-full bg-[#202020] ring-4 ring-[#EFEFED]" />
          <div className="ml-2 px-1.5 py-0.5 rounded bg-[#202020] text-white text-[10px] font-mono tracking-tight shadow-sm">
            Current: {currentTimeFormatted}
          </div>
          <div className="w-24 h-[1px] bg-[#202020] ml-2" />
        </div>
      )}

      {items.map((item, idx) => {
        const isFixed = item.type === 'fixed_event';
        const isBreak = item.type === 'break';
        const isTask = item.type === 'scheduled_task';

        let nodeBorder = 'border-[#DEDEDA]';
        let bgStyle = 'bg-white';
        let Icon = CheckSquare;

        if (isFixed) {
          Icon = Calendar;
          bgStyle = 'bg-white';
        } else if (isBreak) {
          Icon = Coffee;
          bgStyle = 'bg-[#FAFAF9]';
        }

        return (
          <div key={item.id || idx} className="relative group">
            {/* Timeline Tick Node */}
            <div
              className={`absolute -left-[31px] top-3.5 w-3 h-3 rounded-full border-2 bg-white transition-colors ${
                isFixed
                  ? 'border-[#202020]'
                  : isBreak
                  ? 'border-[#888884]'
                  : 'border-[#777773]'
              }`}
            />

            {/* Content Item Card */}
            <div
              onClick={() => onItemClick && onItemClick(item)}
              className={`p-3.5 rounded-md border ${nodeBorder} ${bgStyle} hover:bg-[#E8E8E5] transition-colors cursor-pointer`}
            >
              <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-2 mb-1.5">
                <div className="flex items-center gap-2">
                  <span className="text-xs font-mono font-medium text-[#202020]">
                    {formatTime12h(item.start)} – {formatTime12h(item.end)}
                  </span>
                  <span className="text-[11px] text-[#777773]">
                    ({formatDuration(item.durationMinutes)})
                  </span>
                </div>

                <div className="flex items-center gap-1.5 flex-wrap">
                  <StatusBadge
                    status={
                      isFixed
                        ? 'Fixed Event'
                        : isBreak
                        ? 'Recuperation Window'
                        : 'Scheduled Task'
                    }
                    size="sm"
                  />
                  {!isBreak && item.priority && (
                    <PriorityBadge priority={item.priority} size="sm" />
                  )}
                </div>
              </div>

              <div className="flex items-center justify-between gap-3">
                <div className="flex items-center gap-2 min-w-0">
                  <Icon className="w-3.5 h-3.5 text-[#777773] flex-shrink-0" />
                  <span className="text-sm font-medium text-[#202020] truncate">
                    {item.title}
                  </span>
                </div>
                <ChevronRight className="w-3.5 h-3.5 text-[#999995] group-hover:text-[#202020] transition-colors flex-shrink-0" />
              </div>

              {item.explanation && (
                <div className="mt-2 text-xs text-[#777773] leading-relaxed border-t border-[#EFEFED] pt-1.5">
                  <span className="font-semibold text-[#555552]">Engine note:</span>{' '}
                  {item.explanation}
                </div>
              )}
            </div>
          </div>
        );
      })}
    </div>
  );
};
