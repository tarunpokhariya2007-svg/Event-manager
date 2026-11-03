import React, { useMemo } from 'react';
import { Link } from 'react-router-dom';
import { SchedulingConflict, AlternativeTimeSuggestion } from '../../types/conflict';
import { useApp } from '../../context/AppContext';
import { AlertTriangle, Clock, Check, ExternalLink, Info } from 'lucide-react';
import { PriorityBadge } from '../common/PriorityBadge';
import { formatTime12h, formatDateReadable, formatDuration } from '../../utils/timeUtils';
import { classifyConflict } from '../../utils/conflictUtils';
import { CalendarEvent } from '../../types/event';

const SEVERITY_STYLE = {
  high: 'bg-[#FEF2F2] text-[#991B1B] border-[#FECACA]',
  medium: 'bg-[#FFFBEB] text-[#92400E] border-[#FDE68A]',
  low: 'bg-[#F7F7F5] text-[#555552] border-[#DEDEDA]',
} as const;

interface ConflictCardProps {
  conflict: SchedulingConflict;
}

export const ConflictCard: React.FC<ConflictCardProps> = ({ conflict }) => {
  const { resolveConflictWithAlternative, tasks, schedulingResult, setSelectedEvent, setSelectedTask, setActiveModal } = useApp();
  const { eventA, eventB, overlapStart, overlapEnd, suggestedAlternatives } = conflict;

  const info = useMemo(() => classifyConflict(conflict), [conflict]);
  const movableEvent = info.movableEventId === eventB.id ? eventB : eventA;

  // Tasks due on the conflict date (real task data) and tasks the latest schedule placed on that date.
  const dueTasks = useMemo(
    () => tasks.filter(t => t.status !== 'Completed' && t.deadline.slice(0, 10) === conflict.date),
    [tasks, conflict.date]
  );
  const placedTasks = useMemo(() => {
    if (!schedulingResult || schedulingResult.targetDate !== conflict.date) return [];
    const ids = new Set(schedulingResult.timeline.filter(i => i.type === 'scheduled_task').map(i => i.referenceId));
    return tasks.filter(t => ids.has(t.id));
  }, [schedulingResult, tasks, conflict.date]);

  const openEvent = (e: CalendarEvent) => {
    setSelectedEvent(e);
    setActiveModal('edit_event');
  };
  const openTask = (id: string) => {
    const t = tasks.find(x => x.id === id);
    if (!t) return;
    setSelectedTask(t);
    setActiveModal('edit_task');
  };

  const handleSelectAlternative = (alt: AlternativeTimeSuggestion) => {
    resolveConflictWithAlternative(conflict.id, movableEvent.id, alt);
  };

  return (
    <div className="bg-white border border-[#DEDEDA] rounded-lg shadow-xs overflow-hidden">
      {/* Conflict Header */}
      <div className="p-4 bg-[#FFFBEB] border-b border-[#FDE68A] flex items-start gap-3">
        <div className="w-8 h-8 rounded-full bg-white border border-[#FDE68A] flex items-center justify-center text-[#92400E] flex-shrink-0 mt-0.5">
          <AlertTriangle className="w-4 h-4" />
        </div>
        <div className="flex-1 min-w-0">
          <div className="flex items-center gap-2">
            <h3 className="text-sm font-semibold text-[#92400E]">
              Schedule Conflict Detected
            </h3>
            <span title={info.reason} className={`text-[10px] font-semibold uppercase px-1.5 py-0.2 rounded border ${SEVERITY_STYLE[info.severity]}`}>
              {info.label}
            </span>
            <span className="text-[11px] font-mono bg-white/70 border border-[#FDE68A] text-[#78350F] px-1.5 py-0.2 rounded">
              Overlap: {formatTime12h(overlapStart)} – {formatTime12h(overlapEnd)} ({formatDuration(info.overlapMinutes)})
            </span>
          </div>
          <p className="text-xs text-[#78350F] mt-1 leading-normal">
            &ldquo;{eventB.title}&rdquo; conflicts with &ldquo;{eventA.title}&rdquo; on {formatDateReadable(conflict.date)}. {info.reason}
          </p>
        </div>
      </div>

      {/* Conflicting Events Comparison Box */}
      <div className="p-5 grid grid-cols-1 md:grid-cols-2 gap-4 border-b border-[#DEDEDA] bg-[#FAFAF9]/50 text-xs">
        {/* Event A */}
        <div className="p-3.5 bg-white border border-[#DEDEDA] rounded-md space-y-2">
          <div className="flex items-center justify-between">
            <span className="text-[10px] uppercase font-bold tracking-wider text-[#777773]">
              {eventA.flexibility === 'Fixed' ? 'Fixed Anchor Event' : 'Movable Event'}
            </span>
            <PriorityBadge priority={eventA.priority} size="sm" />
          </div>
          <button onClick={() => openEvent(eventA)} title="Open this event" className="text-sm font-semibold text-[#202020] truncate hover:underline text-left max-w-full">
            {eventA.title}
          </button>
          <div className="flex items-center gap-2 text-xs font-mono text-[#555552]">
            <Clock className="w-3.5 h-3.5 text-[#777773]" />
            <span>{formatTime12h(eventA.startTime)} – {formatTime12h(eventA.endTime)}</span>
          </div>
          <div className="text-[11px] text-[#777773]">
            Flexibility: <strong className="text-[#202020]">{eventA.flexibility}</strong>
            {eventA.location && ` • ${eventA.location}`}
          </div>
        </div>

        {/* Event B */}
        <div className="p-3.5 bg-white border border-[#DEDEDA] rounded-md space-y-2">
          <div className="flex items-center justify-between">
            <span className="text-[10px] uppercase font-bold tracking-wider text-[#777773]">
              {eventB.flexibility === 'Fixed' ? 'Fixed Anchor Event' : 'Flexible Candidate'}
            </span>
            <PriorityBadge priority={eventB.priority} size="sm" />
          </div>
          <button onClick={() => openEvent(eventB)} title="Open this event" className="text-sm font-semibold text-[#202020] truncate hover:underline text-left max-w-full">
            {eventB.title}
          </button>
          <div className="flex items-center gap-2 text-xs font-mono text-[#555552]">
            <Clock className="w-3.5 h-3.5 text-[#777773]" />
            <span>{formatTime12h(eventB.startTime)} – {formatTime12h(eventB.endTime)}</span>
          </div>
          <div className="text-[11px] text-[#777773]">
            Flexibility: <strong className="text-[#202020]">{eventB.flexibility}</strong>
            {eventB.location && ` • ${eventB.location}`}
          </div>
        </div>
      </div>

      {/* Affected tasks */}
      <div className="px-5 py-3 border-b border-[#DEDEDA] text-[11px] text-[#555552] space-y-1">
        <div>
          <span className="font-semibold text-[#202020]">Tasks due this day: </span>
          {dueTasks.length === 0 ? 'none' : dueTasks.map((t, i) => (
            <React.Fragment key={t.id}>
              {i > 0 && ', '}
              <button onClick={() => openTask(t.id)} className="underline hover:text-black">{t.title}</button>
            </React.Fragment>
          ))}
        </div>
        {placedTasks.length > 0 && (
          <div>
            <span className="font-semibold text-[#202020]">Placed on this day in your latest schedule: </span>
            {placedTasks.map((t, i) => (
              <React.Fragment key={t.id}>
                {i > 0 && ', '}
                <button onClick={() => openTask(t.id)} className="underline hover:text-black">{t.title}</button>
              </React.Fragment>
            ))}
            . <Link to="/planner" className="underline">Regenerate in the planner</Link> after resolving.
          </div>
        )}
      </div>

      {/* Suggested Alternative Time Slots (3 alternatives) */}
      <div className="p-5 text-xs">
        <div className="mb-3">
          <h4 className="text-xs font-semibold text-[#202020] uppercase tracking-wider">
            Suggested Alternative Slots for &ldquo;{movableEvent.title}&rdquo;
          </h4>
          <p className="text-[#777773] text-[11px] mt-0.5">
            Free gaps in your work window on this date, computed from your calendar events (flexible tasks are not considered).
          </p>
        </div>

        {suggestedAlternatives.length === 0 && (
          <div className="flex items-start gap-2 p-3 bg-[#F7F7F5] border border-[#E8E8E5] rounded-md text-[11px] text-[#555552]">
            <Info className="w-3.5 h-3.5 mt-0.5 flex-shrink-0" />
            <span>
              No free gap in your work window is long enough for &ldquo;{movableEvent.title}&rdquo;. Edit one of the events, move it to another day, or widen your work hours in Settings.{' '}
              <button onClick={() => openEvent(movableEvent)} className="underline font-medium">
                <ExternalLink className="inline w-3 h-3" /> Edit event
              </button>
            </span>
          </div>
        )}

        <div className="grid grid-cols-1 md:grid-cols-3 gap-3">
          {suggestedAlternatives.map(alt => (
            <div
              key={alt.id}
              className={`p-3.5 rounded-md border flex flex-col justify-between transition-all ${
                alt.recommended
                  ? 'border-[#202020] bg-white ring-1 ring-[#202020]'
                  : 'border-[#DEDEDA] bg-white hover:bg-[#F7F7F5]'
              }`}
            >
              <div>
                <div className="flex items-center justify-between mb-1.5">
                  <span className="text-xs font-mono font-semibold text-[#202020]">
                    {alt.label}
                  </span>
                  {alt.recommended && (
                    <span className="px-1.5 py-0.2 rounded text-[10px] uppercase font-bold tracking-wider bg-[#202020] text-white">
                      Recommended
                    </span>
                  )}
                </div>
                <p className="text-[11px] text-[#777773] leading-relaxed">
                  {alt.rationale}
                </p>
              </div>

              <button
                onClick={() => handleSelectAlternative(alt)}
                className={`mt-4 w-full py-1.5 px-3 rounded text-xs font-medium transition-colors flex items-center justify-center gap-1.5 ${
                  alt.recommended
                    ? 'bg-[#202020] hover:bg-[#333330] text-white'
                    : 'bg-[#EFEFED] hover:bg-[#E3E3DF] text-[#202020] border border-[#DEDEDA]'
                }`}
              >
                <Check className="w-3.5 h-3.5" />
                <span>Select &amp; Reschedule</span>
              </button>
            </div>
          ))}
        </div>
      </div>
    </div>
  );
};
