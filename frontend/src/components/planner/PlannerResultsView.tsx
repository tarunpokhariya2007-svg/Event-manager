import React, { useMemo, useState } from 'react';
import { Link } from 'react-router-dom';
import { useApp } from '../../context/AppContext';
import {
  Sparkles,
  Calendar,
  CheckSquare,
  AlertTriangle,
  Clock,
  HelpCircle,
  CalendarCheck,
  Loader2,
  History,
  Ban,
  ShieldCheck,
  ExternalLink
} from 'lucide-react';
import { formatTime12h, formatDuration, formatDateReadable } from '../../utils/timeUtils';
import { PriorityBadge } from '../common/PriorityBadge';
import { ScheduleTimeline } from '../schedule/ScheduleTimeline';
import { ScheduleHistoryEntry, ScheduledTimelineItem } from '../../types/planner';
import {
  PLACEMENT_META,
  RISK_CATEGORY_META,
  appliedConstraints,
  computeBlockedIds,
  computePlacements,
  humanizeCode,
  prerequisitesOf,
  PlacementState
} from '../../utils/plannerInsights';

interface Props {
  /** When set, a past schedule from the local history is shown instead of the current one. */
  historicalEntry?: ScheduleHistoryEntry | null;
  onExitHistory?: () => void;
}

const PlacementBadge: React.FC<{ state: PlacementState }> = ({ state }) => (
  <span
    title={PLACEMENT_META[state].hint}
    className={`px-1.5 py-0.5 rounded border text-[10px] font-semibold whitespace-nowrap ${PLACEMENT_META[state].style}`}
  >
    {PLACEMENT_META[state].label}
  </span>
);

export const PlannerResultsView: React.FC<Props> = ({ historicalEntry = null, onExitHistory }) => {
  const { schedulingResult, scheduleError, isGeneratingSchedule, applyScheduleToCalendar, tasks, events, setActiveModal, setSelectedTask, setSelectedEvent } = useApp();
  const [focus, setFocus] = useState<ScheduledTimelineItem | null>(null);

  const result = historicalEntry ? historicalEntry.result : schedulingResult;
  const isHistorical = !!historicalEntry;

  const tasksById = useMemo(() => new Map(tasks.map(t => [t.id, t])), [tasks]);
  const eventsById = useMemo(() => new Map(events.map(e => [e.id, e])), [events]);
  const placements = useMemo(() => (result ? computePlacements(result) : null), [result]);
  const blockedIds = useMemo(
    () => (result && placements ? computeBlockedIds(result, tasksById, placements) : new Set<string>()),
    [result, placements, tasksById]
  );
  const constraints = useMemo(() => (result ? appliedConstraints(result) : []), [result]);

  const openTask = (id: string) => {
    const t = tasksById.get(id);
    if (!t) return;
    setSelectedTask(t);
    setActiveModal('edit_task');
  };
  const openEvent = (id: string) => {
    const e = eventsById.get(id);
    if (!e) return;
    setSelectedEvent(e);
    setActiveModal('edit_event');
  };

  if (!result && isGeneratingSchedule) {
    return (
      <div className="bg-white border border-[#DEDEDA] rounded-lg p-12 text-center" role="status" aria-live="polite">
        <Loader2 className="w-6 h-6 mx-auto mb-3 animate-spin text-[#555552]" />
        <h3 className="text-sm font-semibold text-[#202020] mb-1">Generating schedule…</h3>
        <p className="text-xs text-[#777773]">The C++ engine is placing your tasks around fixed events.</p>
      </div>
    );
  }

  if (!result && scheduleError) {
    return (
      <div className="bg-[#FEF2F2] border border-[#FECACA] rounded-lg p-6 text-xs text-[#991B1B]">
        <div className="flex items-center gap-2 font-semibold text-sm mb-1">
          <AlertTriangle className="w-4 h-4" />
          <span>Schedule could not be generated</span>
        </div>
        <p className="leading-relaxed">{scheduleError}</p>
        <p className="mt-2 text-[#7F1D1D]">No simulated schedule is shown. Fix the problem above and generate again.</p>
      </div>
    );
  }

  if (!result || !placements) {
    return (
      <div className="bg-white border border-[#DEDEDA] rounded-lg p-12 text-center">
        <div className="w-12 h-12 rounded-full bg-[#EFEFED] text-[#555552] flex items-center justify-center mx-auto mb-3">
          <Sparkles className="w-6 h-6 stroke-[1.5]" />
        </div>
        <h3 className="text-base font-semibold text-[#202020] mb-1">No Schedule Generated Yet</h3>
        <p className="text-xs text-[#777773] max-w-md mx-auto leading-relaxed">
          Configure your working hours above, then click &ldquo;Generate Schedule&rdquo;. You will see where each task was placed, why, and what could not be scheduled.
        </p>
      </div>
    );
  }

  const {
    timeline, availableSlots, fixedEvents, allocatedTasksCount, unscheduledTasks, tasksAtRisk, conflicts,
    totalAllocatedMinutes, totalAvailableMinutes, status, warnings, statistics, solverDurationMs, objective
  } = result;

  const statusLabel = status === 'success' ? 'Success' : status === 'partial' ? 'Partial schedule' : 'Infeasible';
  const statusStyle =
    status === 'success'
      ? 'bg-[#F0FDF4] border-[#BBF7D0] text-[#166534]'
      : status === 'partial'
      ? 'bg-[#FFFBEB] border-[#FDE68A] text-[#92400E]'
      : 'bg-[#FEF2F2] border-[#FECACA] text-[#991B1B]';

  const counts = { scheduled: 0, partial: 0, unscheduled: 0 };
  placements.forEach(p => { counts[p.state] += 1; });
  const overviewRows = [...placements.values()].sort((a, b) => {
    const order = { unscheduled: 0, partial: 1, scheduled: 2 } as const;
    return order[a.state] - order[b.state] || a.title.localeCompare(b.title);
  });

  const focusTask = focus?.type === 'scheduled_task' ? tasksById.get(focus.referenceId) : undefined;
  const focusPlacement = focus ? placements.get(focus.referenceId) : undefined;
  const focusPrereqs = focusTask ? prerequisitesOf(focusTask, tasksById, placements) : [];

  const summaryTiles: Array<{ label: string; value: number; hint: string; style: string }> = [
    { label: 'Scheduled', value: counts.scheduled, hint: PLACEMENT_META.scheduled.hint, style: 'text-[#166534]' },
    { label: 'Partial', value: counts.partial, hint: PLACEMENT_META.partial.hint, style: 'text-[#92400E]' },
    { label: 'Unscheduled', value: counts.unscheduled, hint: PLACEMENT_META.unscheduled.hint, style: 'text-[#991B1B]' },
    { label: 'At risk', value: tasksAtRisk.length, hint: 'Tasks the engine flagged for deadline, capacity or dependency risk.', style: 'text-[#92400E]' },
    { label: 'Blocked', value: blockedIds.size, hint: 'Tasks held back by an unfinished or unplaced prerequisite.', style: 'text-[#202020]' },
  ];

  return (
    <div className={`space-y-6 transition-opacity ${isGeneratingSchedule && !isHistorical ? 'opacity-60' : ''}`}>
      {isHistorical && (
        <div className="bg-[#EFEFED] border border-[#DEDEDA] rounded-lg p-3 flex flex-col md:flex-row md:items-center justify-between gap-2 text-xs text-[#202020]">
          <span className="flex items-center gap-2">
            <History className="w-3.5 h-3.5" />
            <span>
              <strong>Historical preview</strong> — generated {new Date(historicalEntry!.savedAt).toLocaleString([], { dateStyle: 'medium', timeStyle: 'short' })}. Task and event data may have changed since; this is not your current schedule.
            </span>
          </span>
          <button onClick={onExitHistory} className="px-3 py-1 rounded-md bg-[#202020] text-white font-medium hover:bg-[#333330] self-start md:self-auto">
            Back to current
          </button>
        </div>
      )}
      {isGeneratingSchedule && !isHistorical && (
        <div className="flex items-center gap-2 text-xs text-[#555552]" role="status">
          <Loader2 className="w-3.5 h-3.5 animate-spin" /> Regenerating schedule…
        </div>
      )}

      {/* Engine banner */}
      <div className="bg-[#FAFAF9] border border-[#DEDEDA] rounded-lg p-4 flex flex-col md:flex-row md:items-center justify-between gap-3 text-xs">
        <div className="flex items-start gap-2.5">
          <div className="w-5 h-5 rounded bg-[#202020] text-white flex items-center justify-center text-[10px] font-bold mt-0.5">C++</div>
          <div>
            <div className="flex items-center gap-2 flex-wrap">
              <span className="font-semibold text-[#202020]">{isHistorical ? 'Past Schedule' : 'C++ Engine Optimization Plan'}</span>
              <span className="px-1.5 py-0.2 rounded bg-[#EFEFED] border border-[#DEDEDA] text-[10px] font-mono text-[#555552]">
                Native C++ engine • {objective} • {solverDurationMs.toFixed(2)} ms
              </span>
              <span className={`px-1.5 py-0.2 rounded border text-[10px] font-semibold ${statusStyle}`}>{statusLabel}</span>
            </div>
            <p className="text-[#777773] text-[11px] mt-0.5">
              Target Date: {formatDateReadable(result.targetDate)} • Generated at {result.generatedAt} • Workload Allocated: {formatDuration(totalAllocatedMinutes)} of {formatDuration(totalAvailableMinutes)} available
            </p>
          </div>
        </div>
        {!isHistorical && (
          <button
            onClick={applyScheduleToCalendar}
            title="Preview only: the backend has no apply endpoint, so this does not change your calendar."
            className="inline-flex items-center gap-1.5 px-3.5 py-1.5 bg-[#202020] hover:bg-[#333330] text-white font-medium rounded-md transition-colors flex-shrink-0"
          >
            <CalendarCheck className="w-3.5 h-3.5" />
            <span>Apply to My Schedule</span>
          </button>
        )}
      </div>

      {/* Placement summary */}
      <div className="grid grid-cols-2 md:grid-cols-5 gap-3">
        {summaryTiles.map(t => (
          <div key={t.label} title={t.hint} className="p-3 bg-white border border-[#DEDEDA] rounded-md">
            <div className="text-[11px] uppercase tracking-wider font-medium text-[#777773]">{t.label}</div>
            <div className={`text-xl font-semibold ${t.style}`}>{t.value}</div>
          </div>
        ))}
      </div>

      {warnings.length > 0 && (
        <div className="bg-[#FFFBEB] border border-[#FDE68A] rounded-lg p-4 text-xs text-[#78350F]">
          <div className="font-semibold mb-1 flex items-center gap-1.5">
            <AlertTriangle className="w-3.5 h-3.5" />
            <span>Engine warnings ({warnings.length})</span>
          </div>
          <ul className="list-disc pl-5 space-y-0.5">
            {warnings.map((w, i) => <li key={i}>{w}</li>)}
          </ul>
        </div>
      )}

      {/* Constraints that shaped this schedule */}
      <div className="bg-white border border-[#DEDEDA] rounded-lg p-4 text-xs">
        <div className="flex items-center gap-1.5 font-medium uppercase tracking-wider text-[11px] text-[#777773] mb-3">
          <ShieldCheck className="w-3.5 h-3.5" />
          <span>Constraints applied to this schedule</span>
        </div>
        <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-3">
          {constraints.map(c => (
            <div key={c.label} title={c.detail} className={`p-3 rounded-md border ${c.warn ? 'bg-[#FFFBEB] border-[#FDE68A]' : 'bg-[#FAFAF9] border-[#E8E8E5]'}`}>
              <div className="text-[11px] text-[#777773]">{c.label}</div>
              <div className="font-semibold text-[#202020]">{c.value}</div>
              <p className="text-[11px] text-[#777773] leading-snug mt-0.5">{c.detail}</p>
            </div>
          ))}
        </div>
      </div>

      {Object.keys(statistics).length > 0 && (
        <details className="bg-white border border-[#DEDEDA] rounded-lg p-4 text-xs">
          <summary className="cursor-pointer font-medium uppercase tracking-wider text-[11px] text-[#777773]">Scheduling statistics</summary>
          <div className="grid grid-cols-2 md:grid-cols-4 gap-x-4 gap-y-1.5 text-[11px] text-[#555552] mt-3">
            {Object.entries(statistics).map(([key, value]) => (
              <div key={key} className="flex items-center justify-between gap-2">
                <span className="truncate">{key.replace(/_/g, ' ')}</span>
                <span className="font-mono text-[#202020]">{Number.isInteger(value) ? value : value.toFixed(1)}</span>
              </div>
            ))}
          </div>
        </details>
      )}

      {/* Overview cards */}
      <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
        <div className="bg-white border border-[#DEDEDA] rounded-lg p-4 flex flex-col justify-between">
          <div>
            <div className="flex items-center justify-between text-xs text-[#777773] mb-2">
              <span className="font-medium uppercase tracking-wider text-[11px]">Available Time Slots</span>
              <Clock className="w-3.5 h-3.5" />
            </div>
            <div className="text-xl font-semibold text-[#202020]">{availableSlots.length} Free Windows</div>
            <div className="text-xs text-[#777773] mt-1">{formatDuration(totalAvailableMinutes)} of uncommitted time between fixed commitments.</div>
          </div>
          <div className="mt-3 pt-3 border-t border-[#EFEFED] space-y-1">
            {availableSlots.length === 0 && <p className="text-[11px] text-[#777773]">No free window remains in the work window.</p>}
            {availableSlots.slice(0, 3).map(slot => (
              <div key={slot.id} className="flex items-center justify-between text-[11px] text-[#555552]">
                <span className="font-mono">{formatTime12h(slot.start)} – {formatTime12h(slot.end)}</span>
                <span className="text-[#777773]">{formatDuration(slot.durationMinutes)}</span>
              </div>
            ))}
          </div>
        </div>

        <div className="bg-white border border-[#DEDEDA] rounded-lg p-4 flex flex-col justify-between">
          <div>
            <div className="flex items-center justify-between text-xs text-[#777773] mb-2">
              <span className="font-medium uppercase tracking-wider text-[11px]">Fixed Calendar Anchors</span>
              <Calendar className="w-3.5 h-3.5" />
            </div>
            <div className="text-xl font-semibold text-[#202020]">{fixedEvents.length} Fixed Events</div>
            <div className="text-xs text-[#777773] mt-1">Commitments that block their time range for task placement.</div>
          </div>
          <div className="mt-3 pt-3 border-t border-[#EFEFED] space-y-1">
            {fixedEvents.length === 0 && <p className="text-[11px] text-[#777773]">No events on this date.</p>}
            {fixedEvents.map(evt => (
              <button
                key={evt.id}
                onClick={() => !isHistorical && openEvent(evt.id)}
                disabled={isHistorical}
                className="w-full flex items-center justify-between text-[11px] text-[#555552] truncate hover:text-black disabled:hover:text-[#555552]"
              >
                <span className="font-medium truncate">{evt.title}</span>
                <span className="font-mono text-[#777773] flex-shrink-0 ml-1">{formatTime12h(evt.startTime)}</span>
              </button>
            ))}
          </div>
        </div>

        <div className="bg-white border border-[#DEDEDA] rounded-lg p-4 flex flex-col justify-between">
          <div>
            <div className="flex items-center justify-between text-xs text-[#777773] mb-2">
              <span className="font-medium uppercase tracking-wider text-[11px]">Placed Tasks</span>
              <CheckSquare className="w-3.5 h-3.5" />
            </div>
            <div className="text-xl font-semibold text-[#202020]">{allocatedTasksCount} Tasks Placed</div>
            <div className="text-xs text-[#777773] mt-1">Tasks with at least one block on the timeline, ordered by the “{objective}” objective.</div>
          </div>
          <div className="mt-3 pt-3 border-t border-[#EFEFED] flex items-center justify-between text-[11px] text-[#555552]">
            <span>Packed Workload</span>
            <span className="font-semibold text-[#202020]">{formatDuration(totalAllocatedMinutes)}</span>
          </div>
        </div>
      </div>

      {/* Timeline + explanation panels */}
      <div className="grid grid-cols-1 lg:grid-cols-12 gap-6">
        <div className="lg:col-span-7 bg-white border border-[#DEDEDA] rounded-lg p-5 min-w-0">
          <div className="flex items-center justify-between pb-3 border-b border-[#DEDEDA] mb-5">
            <div>
              <h3 className="text-sm font-semibold text-[#202020]">{isHistorical ? 'Timeline (historical)' : 'Optimized Daily Timeline'}</h3>
              <p className="text-xs text-[#777773] mt-0.5">Click any block to see why the engine placed it there.</p>
            </div>
            <span className="text-[11px] font-mono text-[#777773]">{timeline.length} Blocks</span>
          </div>
          {timeline.length === 0 ? (
            <p className="text-xs text-[#777773] text-center py-8">The timeline is empty: nothing was placed on this date.</p>
          ) : (
            <ScheduleTimeline items={timeline} showCurrentTimeIndicator={false} onItemClick={item => setFocus(item)} />
          )}
        </div>

        <div className="lg:col-span-5 space-y-4 min-w-0">
          {/* Why was this scheduled here */}
          <div className="bg-white border border-[#DEDEDA] rounded-lg p-4">
            <div className="flex items-center gap-2 pb-2.5 border-b border-[#DEDEDA] mb-3">
              <HelpCircle className="w-4 h-4 text-[#555552]" />
              <h4 className="text-xs font-semibold text-[#202020] uppercase tracking-wider">Why was this scheduled here?</h4>
            </div>
            <div className="p-3 bg-[#F7F7F5] border border-[#E8E8E5] rounded-md text-xs space-y-2">
              {!focus ? (
                <p className="text-[#555552] leading-relaxed">Select a block in the timeline to see the placement explanation returned by the C++ engine.</p>
              ) : (
                <>
                  <div className="flex items-center justify-between gap-2">
                    <span className="font-semibold text-[#202020]">&ldquo;{focus.title}&rdquo;</span>
                    <span className="font-mono text-[11px] text-[#777773]">{formatTime12h(focus.start)} – {formatTime12h(focus.end)}</span>
                  </div>
                  <p className="text-[#555552] leading-relaxed">{focus.explanation || 'The engine returned no explanation for this block.'}</p>
                  {focusPlacement && (
                    <div className="flex items-center gap-2 flex-wrap text-[11px] text-[#555552]">
                      <PlacementBadge state={focusPlacement.state} />
                      <span>
                        {formatDuration(focusPlacement.placedMinutes)} placed in {focusPlacement.sessions} session{focusPlacement.sessions === 1 ? '' : 's'}
                        {focusPlacement.remainingMinutes ? `, ${formatDuration(focusPlacement.remainingMinutes)} remaining` : ''}
                      </span>
                    </div>
                  )}
                  {focusTask && focusTask.deadline && (
                    <div className="text-[11px] text-[#555552]">Deadline: <span className="font-mono">{focusTask.deadline.replace('T', ' ')}</span></div>
                  )}
                  {focusPrereqs.length > 0 && (
                    <div className="text-[11px] text-[#555552]">
                      Prerequisites:{' '}
                      {focusPrereqs.map(p => `${p.task.title} (${p.completed ? 'completed' : p.placement ? PLACEMENT_META[p.placement.state].label.toLowerCase() : 'not in this schedule'})`).join(', ')}
                    </div>
                  )}
                  {!isHistorical && (
                    <button
                      onClick={() => (focus.type === 'scheduled_task' ? openTask(focus.referenceId) : openEvent(focus.referenceId))}
                      className="inline-flex items-center gap-1 text-[11px] font-medium underline text-[#202020]"
                    >
                      <ExternalLink className="w-3 h-3" /> Open {focus.type === 'scheduled_task' ? 'task' : 'event'}
                    </button>
                  )}
                </>
              )}
            </div>
          </div>

          {/* Task placement overview */}
          <div className="bg-white border border-[#DEDEDA] rounded-lg p-4">
            <div className="flex items-center justify-between pb-2.5 border-b border-[#DEDEDA] mb-3">
              <span className="uppercase tracking-wider text-[11px] font-semibold text-[#202020]">Task placement ({placements.size})</span>
            </div>
            {overviewRows.length === 0 ? (
              <p className="text-[11px] text-[#777773]">No flexible tasks were considered. Add tasks or enable &ldquo;include pending tasks&rdquo;.</p>
            ) : (
              <ul className="space-y-1.5 max-h-64 overflow-y-auto">
                {overviewRows.map(p => (
                  <li key={p.taskId} className="flex items-center justify-between gap-2 text-[11px]">
                    <span className="truncate text-[#202020] font-medium">
                      {blockedIds.has(p.taskId) && <Ban className="inline w-3 h-3 mr-1 text-[#92400E]" aria-label="Blocked by dependency" />}
                      {p.title}
                    </span>
                    <span className="flex items-center gap-2 flex-shrink-0">
                      <span className="font-mono text-[#777773]">{formatDuration(p.placedMinutes)}{p.remainingMinutes ? ` / +${formatDuration(p.remainingMinutes)}` : ''}</span>
                      <PlacementBadge state={p.state} />
                    </span>
                  </li>
                ))}
              </ul>
            )}
          </div>

          {/* Unscheduled */}
          <div className="bg-white border border-[#DEDEDA] rounded-lg p-4">
            <div className="flex items-center gap-1.5 pb-2.5 border-b border-[#DEDEDA] mb-3 text-xs font-semibold text-[#202020]">
              <AlertTriangle className="w-3.5 h-3.5 text-[#92400E]" />
              <span className="uppercase tracking-wider text-[11px]">Why tasks were not fully scheduled ({unscheduledTasks.length})</span>
            </div>
            <div className="space-y-2.5">
              {unscheduledTasks.length === 0 && <p className="text-[11px] text-[#777773]">Every considered task was placed.</p>}
              {unscheduledTasks.map(item => {
                const placement = placements.get(item.task.id);
                const task = tasksById.get(item.task.id) ?? item.task;
                const unmet = prerequisitesOf(task, tasksById, placements).filter(p => !p.completed);
                return (
                  <div key={item.task.id} className="p-3 bg-[#FFFDF7] border border-[#FDE68A] rounded-md text-xs space-y-1">
                    <div className="flex items-center justify-between gap-2">
                      <span className="font-semibold text-[#202020] truncate">{item.task.title}</span>
                      <PriorityBadge priority={item.task.priority} size="sm" />
                    </div>
                    <div className="flex items-center gap-2 flex-wrap">
                      {placement && <PlacementBadge state={placement.state} />}
                      {item.reasonCode && (
                        <span className="px-1.5 rounded bg-[#EFEFED] border border-[#DEDEDA] text-[10px] font-mono text-[#555552]">{humanizeCode(item.reasonCode)}</span>
                      )}
                      {item.remainingMinutes ? <span className="text-[11px] font-mono text-[#777773]">{formatDuration(item.remainingMinutes)} unplaced</span> : null}
                    </div>
                    <div className="text-[11px] font-medium text-[#92400E]">{item.reason}</div>
                    {unmet.length > 0 && (
                      <div className="text-[11px] text-[#78350F]">
                        Waiting on:{' '}
                        {unmet.map(u => `${u.task.title} (${u.placement ? PLACEMENT_META[u.placement.state].label.toLowerCase() : 'not completed'})`).join(', ')}
                      </div>
                    )}
                    <p className="text-[11px] text-[#777773] leading-normal pt-1 border-t border-[#FEEFB3]">
                      <span className="font-medium text-[#555552]">Suggestion:</span> {item.suggestedAction}
                    </p>
                    {!isHistorical && tasksById.has(item.task.id) && (
                      <button onClick={() => openTask(item.task.id)} className="inline-flex items-center gap-1 text-[11px] font-medium underline text-[#202020]">
                        <ExternalLink className="w-3 h-3" /> Edit task
                      </button>
                    )}
                  </div>
                );
              })}
            </div>
          </div>

          {/* At risk */}
          <div className="bg-white border border-[#DEDEDA] rounded-lg p-4">
            <div className="flex items-center gap-1.5 pb-2.5 border-b border-[#DEDEDA] mb-3 text-xs font-semibold text-[#202020]">
              <Clock className="w-3.5 h-3.5 text-[#555552]" />
              <span className="uppercase tracking-wider text-[11px]">Deadline risks ({tasksAtRisk.length})</span>
            </div>
            <div className="space-y-2.5">
              {tasksAtRisk.length === 0 && <p className="text-[11px] text-[#777773]">No task is at risk of missing its deadline.</p>}
              {tasksAtRisk.map(item => {
                const meta = item.category ? RISK_CATEGORY_META[item.category] : undefined;
                const high = item.riskLevel === 'High';
                return (
                  <div key={item.task.id} className="p-3 bg-[#FDFCFB] border border-[#E8E8E5] rounded-md text-xs space-y-1">
                    <div className="flex items-center justify-between gap-2">
                      <span className="font-semibold text-[#202020] truncate">{item.task.title}</span>
                      <span className={`px-1.5 py-0.2 rounded text-[10px] uppercase font-semibold border ${high ? 'bg-[#FEF2F2] text-[#991B1B] border-[#FECACA]' : 'bg-[#FFFBEB] text-[#92400E] border-[#FDE68A]'}`}>
                        {item.riskLevel} Risk
                      </span>
                    </div>
                    {meta && (
                      <div title={meta.hint} className="text-[11px] font-medium text-[#555552]">{meta.label}</div>
                    )}
                    <p className="text-[11px] text-[#777773] leading-normal">{item.reason}</p>
                    <div className="text-[11px] text-[#555552] font-mono">
                      Buffer to deadline: {item.bufferMinutes < 0 ? `-${formatDuration(-item.bufferMinutes)}` : formatDuration(item.bufferMinutes)}
                    </div>
                    {item.estimatedCompletion && (
                      <div className="text-[11px] text-[#777773] font-mono">
                        Est. completion {item.estimatedCompletion.replace('T', ' ')}
                        {item.deadline ? ` • deadline ${item.deadline.replace('T', ' ')}` : ''}
                      </div>
                    )}
                  </div>
                );
              })}
            </div>
          </div>

          {/* Conflicts */}
          {conflicts.length > 0 && (
            <div className="bg-[#FFFBEB] border border-[#FDE68A] rounded-lg p-4 text-xs">
              <div className="flex items-center justify-between mb-2">
                <span className="font-semibold text-[#92400E] flex items-center gap-1.5">
                  <AlertTriangle className="w-3.5 h-3.5" />
                  <span>Overlapping events ({conflicts.length})</span>
                </span>
                {!isHistorical && (
                  <Link to="/conflicts" className="font-medium underline text-[#78350F] hover:text-black">Resolve →</Link>
                )}
              </div>
              <ul className="space-y-1 text-[11px] text-[#78350F]">
                {conflicts.map(c => (
                  <li key={c.id}>
                    &ldquo;{c.eventA.title}&rdquo; and &ldquo;{c.eventB.title}&rdquo; overlap {formatTime12h(c.overlapStart)} – {formatTime12h(c.overlapEnd)}.
                  </li>
                ))}
              </ul>
            </div>
          )}
        </div>
      </div>
    </div>
  );
};
