import React, { useMemo, useState } from 'react';
import { useApp } from '../../context/AppContext';
import { CalendarEvent } from '../../types/event';
import { ScheduledTimelineItem } from '../../types/planner';
import {
  ChevronLeft,
  ChevronRight,
  Plus,
  AlertTriangle,
  Calendar as CalendarIcon,
  Clock,
  Layers
} from 'lucide-react';
import {
  formatTime12h,
  formatDateReadable,
  getTodayDateString,
  parseMinutes,
  formatMinutesToTime
} from '../../utils/timeUtils';
import { PriorityBadge } from '../common/PriorityBadge';
import { StatusBadge } from '../common/StatusBadge';

type CalendarMode = 'day' | 'week' | 'month';

export const CalendarView: React.FC = () => {
  const {
    events,
    tasks,
    conflicts,
    schedulingResult,
    scheduleError,
    setSelectedTask,
    setSelectedEvent,
    setActiveModal,
    openCreateEventWithSlot
  } = useApp();

  const [mode, setMode] = useState<CalendarMode>('week');
  const [currentDateStr, setCurrentDateStr] = useState(getTodayDateString());

  // Hours array from 08:00 to 20:00 for the grid
  const hours = [
    '08:00', '09:00', '10:00', '11:00', '12:00', '13:00',
    '14:00', '15:00', '16:00', '17:00', '18:00', '19:00', '20:00'
  ];

  // Helper to compute week dates centered or starting near currentDateStr
  const getWeekDates = (baseDateStr: string): string[] => {
    const [y, m, d] = baseDateStr.split('-').map(Number);
    const curr = new Date(y, m - 1, d);
    const dayOfWeek = curr.getDay(); // 0 is Sunday
    const distanceToMonday = dayOfWeek === 0 ? -6 : 1 - dayOfWeek;
    
    const monday = new Date(curr);
    monday.setDate(curr.getDate() + distanceToMonday);

    const week: string[] = [];
    for (let i = 0; i < 7; i++) {
      const nextDay = new Date(monday);
      nextDay.setDate(monday.getDate() + i);
      const year = nextDay.getFullYear();
      const month = (nextDay.getMonth() + 1).toString().padStart(2, '0');
      const day = nextDay.getDate().toString().padStart(2, '0');
      week.push(`${year}-${month}-${day}`);
    }
    return week;
  };

  const weekDays = getWeekDates(currentDateStr);

  // Generated allocations (tasks/breaks) from the C++ scheduler. Fixed events come from `events`
  // so they are never rendered twice.
  const generatedItems = useMemo(() => {
    if (!schedulingResult) return [] as ScheduledTimelineItem[];
    return schedulingResult.timeline
      .filter(i => i.type !== 'fixed_event')
      .map(i => ({ ...i, date: i.date ?? schedulingResult.targetDate }));
  }, [schedulingResult]);

  const itemsOn = (dateStr: string) =>
    generatedItems
      .filter(i => i.date === dateStr)
      .sort((a, b) => parseMinutes(a.start) - parseMinutes(b.start));

  const openItem = (item: ScheduledTimelineItem) => {
    if (item.type !== 'scheduled_task') return;
    const t = tasks.find(x => x.id === item.referenceId);
    if (t) {
      setSelectedTask(t);
      setActiveModal('edit_task');
    }
  };

  const itemLabel = (i: ScheduledTimelineItem) =>
    i.isSplitPart && i.totalSplits ? `${i.title} (${(i.splitIndex ?? 0) + 1}/${i.totalSplits})` : i.title;

  const itemClass = (i: ScheduledTimelineItem) =>
    i.type === 'break'
      ? 'bg-[#F2F2EF] text-[#777773] border border-dashed border-[#C8C8C3]'
      : 'bg-white text-[#202020] border border-[#202020] border-l-4 hover:bg-[#EFEFED] cursor-pointer';

  // Month grid (Monday-first) for the month of currentDateStr
  const monthCells = useMemo(() => {
    const [y, m] = currentDateStr.split('-').map(Number);
    const first = new Date(y, m - 1, 1);
    const offset = (first.getDay() + 6) % 7;
    const daysInMonth = new Date(y, m, 0).getDate();
    const total = Math.ceil((offset + daysInMonth) / 7) * 7;
    return Array.from({ length: total }, (_, i) => {
      const dayNum = i - offset + 1;
      if (dayNum < 1 || dayNum > daysInMonth) return null;
      return { dayNum, dateStr: `${y}-${String(m).padStart(2, '0')}-${String(dayNum).padStart(2, '0')}` };
    });
  }, [currentDateStr]);

  const dayEntries = useMemo(() => {
    const evs = events
      .filter(e => e.date === currentDateStr)
      .map(evt => ({ kind: 'event' as const, evt, start: parseMinutes(evt.startTime) }));
    const its = itemsOn(currentDateStr).map(item => ({ kind: 'item' as const, item, start: parseMinutes(item.start) }));
    return [...evs, ...its].sort((a, b) => a.start - b.start);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [events, generatedItems, currentDateStr]);

  const navigateDate = (deltaDays: number) => {
    const [y, m, d] = currentDateStr.split('-').map(Number);
    const next = new Date(y, m - 1, d);
    next.setDate(next.getDate() + deltaDays);
    const year = next.getFullYear();
    const month = (next.getMonth() + 1).toString().padStart(2, '0');
    const day = next.getDate().toString().padStart(2, '0');
    setCurrentDateStr(`${year}-${month}-${day}`);
  };

  const navigateMonth = (delta: number) => {
    const [y, m] = currentDateStr.split('-').map(Number);
    const next = new Date(y, m - 1 + delta, 1);
    setCurrentDateStr(`${next.getFullYear()}-${String(next.getMonth() + 1).padStart(2, '0')}-01`);
  };

  // Check if a date has conflicts
  const hasConflictOnDate = (dateStr: string) => {
    return conflicts.some(c => c.date === dateStr);
  };

  return (
    <div className="bg-white border border-[#DEDEDA] rounded-lg shadow-sm overflow-hidden flex flex-col">
      {/* Calendar Header Controls */}
      <div className="px-5 py-3.5 border-b border-[#DEDEDA] flex flex-col sm:flex-row sm:items-center justify-between gap-3 bg-[#FAFAF9]">
        {/* Navigation & Today */}
        <div className="flex items-center gap-2">
          <button
            onClick={() => setCurrentDateStr(getTodayDateString())}
            className="px-2.5 py-1.5 text-xs font-medium text-[#202020] bg-white border border-[#DEDEDA] rounded-md hover:bg-[#E8E8E5] transition-colors"
          >
            Today
          </button>
          <div className="flex items-center border border-[#DEDEDA] rounded-md bg-white">
            <button
              onClick={() => (mode === 'month' ? navigateMonth(-1) : navigateDate(mode === 'week' ? -7 : -1))}
              className="p-1.5 text-[#555552] hover:text-[#202020] hover:bg-[#E8E8E5] transition-colors"
              aria-label="Previous period"
            >
              <ChevronLeft className="w-3.5 h-3.5" />
            </button>
            <button
              onClick={() => (mode === 'month' ? navigateMonth(1) : navigateDate(mode === 'week' ? 7 : 1))}
              className="p-1.5 text-[#555552] hover:text-[#202020] hover:bg-[#E8E8E5] transition-colors"
              aria-label="Next period"
            >
              <ChevronRight className="w-3.5 h-3.5" />
            </button>
          </div>
          <span className="text-xs font-semibold text-[#202020] ml-1">
            {mode === 'day'
              ? formatDateReadable(currentDateStr)
              : mode === 'week'
              ? `${formatDateReadable(weekDays[0])} – ${formatDateReadable(weekDays[6])}`
              : `Month of ${new Date(`${currentDateStr}T00:00:00`).toLocaleString('default', { month: 'long', year: 'numeric' })}`}
          </span>
        </div>

        {/* View Mode Switcher */}
        <div className="flex items-center gap-1 bg-[#EFEFED] p-0.5 rounded-md border border-[#DEDEDA]">
          <button
            onClick={() => setMode('day')}
            className={`px-3 py-1 text-xs rounded transition-colors ${
              mode === 'day'
                ? 'bg-white font-medium text-[#202020] shadow-xs'
                : 'text-[#555552] hover:text-[#202020]'
            }`}
          >
            Day
          </button>
          <button
            onClick={() => setMode('week')}
            className={`px-3 py-1 text-xs rounded transition-colors ${
              mode === 'week'
                ? 'bg-white font-medium text-[#202020] shadow-xs'
                : 'text-[#555552] hover:text-[#202020]'
            }`}
          >
            Week
          </button>
          <button
            onClick={() => setMode('month')}
            className={`px-3 py-1 text-xs rounded transition-colors ${
              mode === 'month'
                ? 'bg-white font-medium text-[#202020] shadow-xs'
                : 'text-[#555552] hover:text-[#202020]'
            }`}
          >
            Month
          </button>
        </div>
      </div>

      {/* Overlapping warning banner if any detected on viewed dates */}
      {conflicts.length > 0 && (
        <div className="px-5 py-2 bg-[#FFFBEB] border-b border-[#FDE68A] flex items-center justify-between text-xs text-[#92400E]">
          <div className="flex items-center gap-2">
            <AlertTriangle className="w-4 h-4 flex-shrink-0" />
            <span>
              <strong>Overlap alert:</strong> {conflicts.length} overlapping schedule conflict(s) detected.
            </span>
          </div>
          <a
            href="/conflicts"
            className="font-medium underline hover:text-[#78350F]"
          >
            Review Alternatives →
          </a>
        </div>
      )}

      {scheduleError && (
        <div className="px-5 py-2 bg-[#FEF2F2] border-b border-[#FECACA] text-xs text-[#991B1B]">
          Generated schedule unavailable: {scheduleError}
        </div>
      )}

      {/* Week Grid View */}
      {mode === 'week' && (
        <div className="overflow-x-auto">
          <div className="min-w-[760px]">
            {/* Days Header */}
            <div className="grid grid-cols-8 border-b border-[#DEDEDA] bg-[#FAFAF9] text-xs">
              <div className="p-3 text-center text-[#777773] font-medium border-r border-[#DEDEDA]">
                Time
              </div>
              {weekDays.map(dayStr => {
                const dateObj = new Date(dayStr);
                const isToday = dayStr === getTodayDateString();
                const dayName = dateObj.toLocaleDateString('en-US', { weekday: 'short' });
                const dayNum = dateObj.getDate();
                const hasConflict = hasConflictOnDate(dayStr);

                return (
                  <div
                    key={dayStr}
                    className={`p-2.5 text-center border-r border-[#DEDEDA] last:border-r-0 ${
                      isToday ? 'bg-[#F2F2EF]' : ''
                    }`}
                  >
                    <div className="text-[11px] text-[#777773] uppercase">{dayName}</div>
                    <div
                      className={`text-sm font-semibold mt-0.5 inline-block ${
                        isToday
                          ? 'w-6 h-6 rounded-full bg-[#202020] text-white leading-6 text-center'
                          : 'text-[#202020]'
                      }`}
                    >
                      {dayNum}
                    </div>
                    {hasConflict && (
                      <span className="block mt-0.5 text-[10px] text-[#92400E] font-medium">
                        Conflict
                      </span>
                    )}
                  </div>
                );
              })}
            </div>

            {/* Time Rows */}
            <div className="divide-y divide-[#EFEFED]">
              {hours.map(hour => (
                <div key={hour} className="grid grid-cols-8 min-h-[58px]">
                  {/* Hour Label */}
                  <div className="p-2 text-right pr-3 text-[11px] text-[#777773] font-mono border-r border-[#DEDEDA] bg-[#FAFAF9]/50 select-none">
                    {formatTime12h(hour)}
                  </div>

                  {/* Day Columns */}
                  {weekDays.map(dayStr => {
                    const cellHourMin = parseMinutes(hour);
                    const nextHourMin = cellHourMin + 60;

                    // Find events overlapping this hour
                    const cellEvents = events.filter(e => {
                      if (e.date !== dayStr) return false;
                      const s = parseMinutes(e.startTime);
                      const end = parseMinutes(e.endTime);
                      return Math.max(s, cellHourMin) < Math.min(end, nextHourMin);
                    });

                    const cellItems = itemsOn(dayStr).filter(i =>
                      Math.max(parseMinutes(i.start), cellHourMin) < Math.min(parseMinutes(i.end), nextHourMin)
                    );

                    return (
                      <div
                        key={dayStr}
                        onClick={() => {
                          if (cellEvents.length === 0 && cellItems.length === 0) {
                            const nextHourStr = formatMinutesToTime(nextHourMin);
                            openCreateEventWithSlot(dayStr, hour, nextHourStr);
                          }
                        }}
                        className="p-1 border-r border-[#EFEFED] last:border-r-0 hover:bg-[#F7F7F5] transition-colors relative group min-h-[58px]"
                      >
                        {/* Empty slot hover plus button */}
                        {cellEvents.length === 0 && cellItems.length === 0 && (
                          <div className="opacity-0 group-hover:opacity-100 absolute inset-0 flex items-center justify-center transition-opacity pointer-events-none">
                            <span className="text-[10px] text-[#777773] bg-white border border-[#DEDEDA] px-1.5 py-0.5 rounded shadow-xs">
                              + Event
                            </span>
                          </div>
                        )}

                        {/* Render Events */}
                        {cellEvents.map(evt => {
                          const isFixed = evt.flexibility === 'Fixed';
                          return (
                            <div
                              key={evt.id}
                              onClick={e => {
                                e.stopPropagation();
                                setSelectedEvent(evt);
                                setActiveModal('event_details');
                              }}
                              className={`p-1.5 rounded text-[11px] cursor-pointer mb-1 border transition-colors leading-tight ${
                                isFixed
                                  ? 'bg-[#202020] text-white border-[#202020] hover:bg-[#333330]'
                                  : 'bg-[#EAEAE6] text-[#202020] border-[#D5D5D0] hover:bg-[#E3E3DF]'
                              }`}
                            >
                              <div className="font-semibold truncate">{evt.title}</div>
                              <div
                                className={`text-[10px] mt-0.5 truncate ${
                                  isFixed ? 'text-[#CCCCCC]' : 'text-[#777773]'
                                }`}
                              >
                                {formatTime12h(evt.startTime)} – {formatTime12h(evt.endTime)}
                              </div>
                            </div>
                          );
                        })}

                        {/* Generated tasks / breaks */}
                        {cellItems.map(item => (
                          <div
                            key={`${item.id}-${hour}`}
                            onClick={e => {
                              e.stopPropagation();
                              openItem(item);
                            }}
                            className={`p-1.5 rounded text-[11px] mb-1 leading-tight ${itemClass(item)}`}
                          >
                            <div className="font-semibold truncate">{itemLabel(item)}</div>
                            <div className="text-[10px] mt-0.5 truncate text-[#777773]">
                              {formatTime12h(item.start)} – {formatTime12h(item.end)}
                            </div>
                          </div>
                        ))}
                      </div>
                    );
                  })}
                </div>
              ))}
            </div>
          </div>
        </div>
      )}

      {/* Day Mode View */}
      {mode === 'day' && (
        <div className="p-6">
          <div className="max-w-2xl mx-auto space-y-4">
            <div className="flex items-center justify-between pb-3 border-b border-[#DEDEDA]">
              <div>
                <h3 className="text-base font-semibold text-[#202020]">
                  {formatDateReadable(currentDateStr)}
                </h3>
                <span className="text-xs text-[#777773]">
                  {dayEntries.length} scheduled items
                </span>
              </div>
              <button
                onClick={() => openCreateEventWithSlot(currentDateStr, '09:00', '10:00')}
                className="px-3 py-1.5 text-xs font-medium bg-[#202020] text-white rounded-md hover:bg-[#333330] flex items-center gap-1.5"
              >
                <Plus className="w-3.5 h-3.5" />
                <span>Add Event</span>
              </button>
            </div>

            <div className="space-y-3">
              {dayEntries.map(entry =>
                entry.kind === 'event' ? (
                  <div
                    key={`e-${entry.evt.id}`}
                    onClick={() => {
                      setSelectedEvent(entry.evt);
                      setActiveModal('event_details');
                    }}
                    className="p-3.5 border border-[#DEDEDA] rounded-md hover:bg-[#E8E8E5] transition-colors cursor-pointer flex items-center justify-between"
                  >
                    <div>
                      <div className="flex items-center gap-2 mb-1">
                        <span className="text-xs font-mono font-medium text-[#202020]">
                          {formatTime12h(entry.evt.startTime)} – {formatTime12h(entry.evt.endTime)}
                        </span>
                        <PriorityBadge priority={entry.evt.priority} size="sm" />
                        <span className="text-[10px] px-1.5 py-0.5 rounded bg-[#EFEFED] text-[#555552] border border-[#DEDEDA]">
                          {entry.evt.flexibility}
                        </span>
                      </div>
                      <div className="text-sm font-medium text-[#202020]">{entry.evt.title}</div>
                      {entry.evt.location && (
                        <div className="text-xs text-[#777773] mt-0.5">{entry.evt.location}</div>
                      )}
                    </div>
                  </div>
                ) : (
                  <div
                    key={`i-${entry.item.id}`}
                    onClick={() => openItem(entry.item)}
                    className={`p-3.5 rounded-md ${itemClass(entry.item)}`}
                  >
                    <div className="flex items-center gap-2 mb-1">
                      <span className="text-xs font-mono font-medium">
                        {formatTime12h(entry.item.start)} – {formatTime12h(entry.item.end)}
                      </span>
                      {entry.item.type === 'scheduled_task' && <PriorityBadge priority={entry.item.priority} size="sm" />}
                      <span className="text-[10px] px-1.5 py-0.5 rounded bg-[#EFEFED] text-[#555552] border border-[#DEDEDA]">
                        {entry.item.type === 'break' ? 'Break' : 'Scheduled Task'}
                      </span>
                    </div>
                    <div className="text-sm font-medium">{itemLabel(entry.item)}</div>
                    {entry.item.explanation && (
                      <div className="text-xs text-[#777773] mt-0.5">{entry.item.explanation}</div>
                    )}
                  </div>
                )
              )}

              {dayEntries.length === 0 && (
                <div className="text-center py-10 text-xs text-[#777773] border border-dashed border-[#DEDEDA] rounded-md">
                  Nothing scheduled on this date. Click "+ Add Event" to create an event.
                </div>
              )}
            </div>
          </div>
        </div>
      )}

      {/* Month Mode View */}
      {mode === 'month' && (
        <div className="p-6">
          <div className="grid grid-cols-7 gap-2">
            {['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun'].map(day => (
              <div key={day} className="text-center text-xs font-medium text-[#777773] py-2">
                {day}
              </div>
            ))}
            {monthCells.map((cell, i) => {
              if (!cell) return <div key={i} className="min-h-[85px]" />;
              const dayEvts = events.filter(e => e.date === cell.dateStr);
              const dayItems = itemsOn(cell.dateStr);
              const chips = [
                ...dayEvts.map(e => ({ key: `e-${e.id}`, label: e.title, start: parseMinutes(e.startTime), cls: 'bg-[#EFEFED] border-[#DEDEDA]' })),
                ...dayItems.map(it => ({ key: `i-${it.id}`, label: itemLabel(it), start: parseMinutes(it.start), cls: it.type === 'break' ? 'bg-[#F2F2EF] border-dashed border-[#C8C8C3] text-[#777773]' : 'bg-white border-[#202020]' }))
              ].sort((a, b) => a.start - b.start);
              return (
                <div
                  key={cell.dateStr}
                  onClick={() => {
                    setCurrentDateStr(cell.dateStr);
                    setMode('day');
                  }}
                  className="min-h-[85px] p-2 border border-[#DEDEDA] rounded-md hover:bg-[#E8E8E5] transition-colors cursor-pointer bg-white flex flex-col justify-between"
                >
                  <span className="text-xs font-semibold text-[#202020]">{cell.dayNum}</span>
                  <div className="space-y-1 mt-1">
                    {chips.slice(0, 2).map(c => (
                      <div key={c.key} className={`text-[10px] border px-1 py-0.5 rounded truncate text-[#202020] ${c.cls}`}>
                        {c.label}
                      </div>
                    ))}
                    {chips.length > 2 && (
                      <div className="text-[9px] text-[#777773]">+{chips.length - 2} more</div>
                    )}
                  </div>
                </div>
              );
            })}
          </div>
        </div>
      )}

      {/* Unscheduled Tasks (from the latest generated schedule) */}
      {schedulingResult && schedulingResult.unscheduledTasks.length > 0 && (
        <div className="px-5 py-4 border-t border-[#DEDEDA] bg-white">
          <h3 className="text-sm font-semibold text-[#202020] mb-2">
            Unscheduled Tasks ({schedulingResult.unscheduledTasks.length})
          </h3>
          <div className="divide-y divide-[#EFEFED]">
            {schedulingResult.unscheduledTasks.map(u => (
              <div
                key={u.task.id}
                onClick={() => {
                  setSelectedTask(u.task);
                  setActiveModal('edit_task');
                }}
                className="py-2 cursor-pointer hover:bg-[#F7F7F5] px-1 rounded"
              >
                <div className="text-xs font-medium text-[#202020]">{u.task.title}</div>
                <div className="text-[11px] text-[#777773]">{u.reason}</div>
              </div>
            ))}
          </div>
        </div>
      )}

      {/* Footer Legend */}
      <div className="px-5 py-2.5 bg-[#FAFAF9] border-t border-[#DEDEDA] flex items-center justify-between text-xs text-[#777773]">
        <div className="flex items-center gap-4">
          <div className="flex items-center gap-1.5">
            <span className="w-3 h-3 bg-[#202020] rounded-xs inline-block" />
            <span className="text-[11px]">Fixed Calendar Event</span>
          </div>
          <div className="flex items-center gap-1.5">
            <span className="w-3 h-3 bg-[#EAEAE6] border border-[#D5D5D0] rounded-xs inline-block" />
            <span className="text-[11px]">Flexible / Movable Event</span>
          </div>
          <div className="flex items-center gap-1.5">
            <span className="w-3 h-3 bg-white border border-[#202020] border-l-4 rounded-xs inline-block" />
            <span className="text-[11px]">Scheduled Task</span>
          </div>
          <div className="flex items-center gap-1.5">
            <span className="w-3 h-3 bg-[#F2F2EF] border border-dashed border-[#C8C8C3] rounded-xs inline-block" />
            <span className="text-[11px]">Break</span>
          </div>
        </div>
        <span className="text-[11px]">Click any empty time slot to quick-schedule</span>
      </div>
    </div>
  );
};
