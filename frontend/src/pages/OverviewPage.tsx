import React, { useMemo } from 'react';
import { Link } from 'react-router-dom';
import { useApp } from '../context/AppContext';
import { PageHeader } from '../components/common/PageHeader';
import { MetricCard } from '../components/common/MetricCard';
import { PriorityBadge } from '../components/common/PriorityBadge';
import { StatusBadge } from '../components/common/StatusBadge';
import { ScheduleTimeline } from '../components/schedule/ScheduleTimeline';
import {
  Calendar,
  CheckSquare,
  AlertTriangle,
  ArrowRight,
  Sparkles,
  CheckCircle2,
  Circle
} from 'lucide-react';
import {
  formatDateReadable,
  formatDuration,
  formatTime12h,
  getTodayDateString,
  parseMinutes
} from '../utils/timeUtils';
import { ScheduledTimelineItem } from '../types/planner';
import { Task, TaskPriority } from '../types/task';
import { CalendarEvent } from '../types/event';

export const OverviewPage: React.FC = () => {
  const {
    tasks,
    events,
    conflicts,
    settings,
    schedulingResult,
    toggleTaskComplete,
    setActiveModal,
    setSelectedTask,
    setSelectedEvent
  } = useApp();

  const today = getTodayDateString();

  // 4 compact summary metrics
  const scheduledEventsCount = events.filter((e: CalendarEvent) => e.date === today).length;
  const pendingTasksCount = tasks.filter((t: Task) => t.status === 'Pending' || t.status === 'In Progress').length;
  const completedTasksCount = tasks.filter((t: Task) => t.status === 'Completed').length;
  const tasksAtRiskCount = tasks.filter((t: Task) => t.priority === 'High' && t.status !== 'Completed').length;

  // Today's timeline: generated schedule items dated today; otherwise today's real events.
  const todayTimelineItems: ScheduledTimelineItem[] = useMemo(() => {
    if (schedulingResult) {
      const generated = schedulingResult.timeline.filter(
        (i: ScheduledTimelineItem) => (i.date ?? schedulingResult.targetDate) === today
      );
      if (generated.length > 0) return generated;
    }
    return events
      .filter((e: CalendarEvent) => e.date === today)
      .sort((a: CalendarEvent, b: CalendarEvent) => a.startTime.localeCompare(b.startTime))
      .map((e: CalendarEvent): ScheduledTimelineItem => ({
        id: `evt-${e.id}`,
        type: 'fixed_event',
        title: e.title,
        start: e.startTime,
        end: e.endTime,
        durationMinutes: parseMinutes(e.endTime) - parseMinutes(e.startTime),
        priority: e.priority,
        status: 'Fixed Event',
        referenceId: e.id,
        category: e.category,
        location: e.location,
        date: e.date
      }));
  }, [schedulingResult, events, today]);

  // Priority Tasks list (Right Section)
  const priorityTasks = tasks
    .filter((t: Task) => t.status !== 'Completed')
    .sort((a: Task, b: Task) => {
      const pRank: Record<TaskPriority, number> = { High: 3, Medium: 2, Low: 1 };
      return pRank[b.priority] - pRank[a.priority];
    })
    .slice(0, 4);

  // Upcoming events
  const upcomingEvents = events
    .filter((e: CalendarEvent) => e.date >= today)
    .sort((a: CalendarEvent, b: CalendarEvent) => new Date(`${a.date}T${a.startTime}`).getTime() - new Date(`${b.date}T${b.startTime}`).getTime())
    .slice(0, 3);

  return (
    <div className="space-y-6">
      {/* Page Heading & Subtitle */}
      <div>
        <h1 className="text-2xl sm:text-[28px] font-semibold text-[#202020] tracking-tight">
          Good evening, {settings.userName}
        </h1>
        <p className="text-[13px] text-[#777773] mt-1 leading-normal">
          Here is your schedule and what needs your attention.
        </p>
      </div>

      {/* 4 Compact Summary Metrics */}
      <div className="grid grid-cols-2 md:grid-cols-4 gap-3">
        <MetricCard
          label="Scheduled Events"
          value={scheduledEventsCount}
          subtext="Active today"
          icon={Calendar}
        />
        <MetricCard
          label="Pending Tasks"
          value={pendingTasksCount}
          subtext="Awaiting execution"
          icon={CheckSquare}
        />
        <MetricCard
          label="Completed Tasks"
          value={completedTasksCount}
          subtext="Finished tasks"
          icon={CheckCircle2}
        />
        <MetricCard
          label="Tasks At Risk"
          value={tasksAtRiskCount}
          subtext="Urgent deadlines"
          icon={AlertTriangle}
          alert={tasksAtRiskCount > 0}
        />
      </div>

      {/* Main Two-Column Layout */}
      <div className="grid grid-cols-1 lg:grid-cols-12 gap-6">
        {/* LEFT SECTION: Today's Schedule */}
        <div className="lg:col-span-7 bg-white border border-[#DEDEDA] rounded-lg p-5">
          <div className="flex items-center justify-between pb-3 border-b border-[#DEDEDA] mb-5">
            <div>
              <h2 className="text-base font-semibold text-[#202020]">
                Today&apos;s Schedule
              </h2>
              <p className="text-xs text-[#777773] mt-0.5">
                Integrated timeline of fixed events and intelligent task placements.
              </p>
            </div>
            <Link
              to="/schedule"
              className="text-xs font-medium text-[#202020] hover:underline flex items-center gap-1"
            >
              <span>Full Calendar</span>
              <ArrowRight className="w-3 h-3" />
            </Link>
          </div>

          {todayTimelineItems.length === 0 ? (
            <div className="text-center py-10 text-xs text-[#777773] border border-dashed border-[#DEDEDA] rounded-md">
              Nothing scheduled today. <Link to="/tasks" className="font-medium text-[#202020] underline">Add tasks</Link> or <Link to="/schedule" className="font-medium text-[#202020] underline">events</Link> to get started.
            </div>
          ) : (
          <ScheduleTimeline
            items={todayTimelineItems}
            showCurrentTimeIndicator={true}
            onItemClick={(item: ScheduledTimelineItem) => {
              if (item.type === 'fixed_event') {
                const targetEvt = events.find((e: CalendarEvent) => e.id === item.referenceId);
                if (targetEvt) {
                  setSelectedEvent(targetEvt);
                  setActiveModal('event_details');
                }
              } else if (item.type === 'scheduled_task') {
                const targetTask = tasks.find((t: Task) => t.id === item.referenceId);
                if (targetTask) {
                  setSelectedTask(targetTask);
                  setActiveModal('edit_task');
                }
              }
            }}
          />
          )}

          <div className="mt-6 pt-4 border-t border-[#DEDEDA] flex items-center justify-between text-xs text-[#777773]">
            <span>Optimal packing calculated by ChronoPlan</span>
            <Link
              to="/planner"
              className="font-medium text-[#202020] hover:underline flex items-center gap-1"
            >
              <Sparkles className="w-3 h-3" />
              <span>Configure in Smart Planner</span>
            </Link>
          </div>
        </div>

        {/* RIGHT SECTION: Priority Tasks & Alerts */}
        <div className="lg:col-span-5 space-y-5">
          {/* Priority Tasks Card */}
          <div className="bg-white border border-[#DEDEDA] rounded-lg p-5">
            <div className="flex items-center justify-between pb-3 border-b border-[#DEDEDA] mb-3">
              <h2 className="text-base font-semibold text-[#202020]">
                Priority Tasks
              </h2>
              <Link
                to="/tasks"
                className="text-xs text-[#202020] font-medium hover:underline flex items-center gap-1"
              >
                <span>View all tasks</span>
                <ArrowRight className="w-3 h-3" />
              </Link>
            </div>

            <div className="divide-y divide-[#EFEFED]">
              {priorityTasks.map((task: Task) => {
                const isDone = task.status === 'Completed';

                return (
                  <div key={task.id} className="py-2.5 flex items-start gap-2.5">
                    <button
                      onClick={() => toggleTaskComplete(task.id)}
                      className="mt-0.5 text-[#777773] hover:text-[#202020] transition-colors"
                      aria-label="Toggle task status"
                    >
                      {isDone ? (
                        <CheckCircle2 className="w-4 h-4 text-[#166534]" />
                      ) : (
                        <Circle className="w-4 h-4" />
                      )}
                    </button>

                    <div className="flex-1 min-w-0">
                      <div className="flex items-center justify-between gap-2">
                        <span className="text-xs font-semibold text-[#202020] truncate">
                          {task.title}
                        </span>
                        <PriorityBadge priority={task.priority} size="sm" />
                      </div>

                      <div className="flex items-center gap-3 text-[11px] text-[#777773] mt-1 font-mono">
                        <span>Due {task.deadline.split('T')[1] || 'Today'}</span>
                        <span>•</span>
                        <span>{formatDuration(task.estimatedDuration)}</span>
                        <span>•</span>
                        <StatusBadge status={task.status} size="sm" />
                      </div>
                    </div>
                  </div>
                );
              })}
            </div>
          </div>

          {/* Upcoming Events Box */}
          <div className="bg-white border border-[#DEDEDA] rounded-lg p-5">
            <div className="flex items-center justify-between pb-3 border-b border-[#DEDEDA] mb-3">
              <h3 className="text-sm font-semibold text-[#202020]">
                Upcoming Events
              </h3>
              <Link
                to="/events"
                className="text-xs text-[#202020] hover:underline"
              >
                Manage
              </Link>
            </div>

            <div className="space-y-2.5 text-xs">
              {upcomingEvents.map((evt: CalendarEvent) => (
                <div
                  key={evt.id}
                  onClick={() => {
                    setSelectedEvent(evt);
                    setActiveModal('event_details');
                  }}
                  className="p-2.5 rounded-md border border-[#DEDEDA] bg-[#FAFAF9] hover:bg-[#E8E8E5] transition-colors cursor-pointer flex items-center justify-between"
                >
                  <div className="min-w-0">
                    <div className="font-semibold text-[#202020] truncate">{evt.title}</div>
                    <div className="text-[11px] text-[#777773] mt-0.5">
                      {formatDateReadable(evt.date)} • {formatTime12h(evt.startTime)}
                    </div>
                  </div>
                  <span className="text-[10px] px-1.5 py-0.5 rounded bg-white text-[#555552] border border-[#DEDEDA] flex-shrink-0">
                    {evt.category}
                  </span>
                </div>
              ))}
            </div>
          </div>

          {/* Deadline Alerts & Conflict Warnings */}
          {conflicts.length > 0 && (
            <div className="p-4 bg-[#FFFBEB] border border-[#FDE68A] rounded-lg text-xs">
              <div className="flex items-center justify-between font-semibold text-[#92400E] mb-1">
                <div className="flex items-center gap-1.5">
                  <AlertTriangle className="w-3.5 h-3.5" />
                  <span>Overlap Alert ({conflicts.length})</span>
                </div>
                <Link to="/conflicts" className="underline hover:text-[#78350F]">
                  Resolve
                </Link>
              </div>
              <p className="text-[11px] text-[#78350F] leading-normal">
                {conflicts[0].eventA.title} overlaps with {conflicts[0].eventB.title}. ChronoPlan has generated 3 conflict-free alternative time slots.
              </p>
            </div>
          )}

          {/* Compact Productivity Summary */}
          <div className="bg-white border border-[#DEDEDA] rounded-lg p-4 text-xs space-y-2">
            <div className="flex items-center justify-between text-[#777773] text-[11px] uppercase tracking-wider font-semibold">
              <span>Today&apos;s Capacity</span>
              <span>7.5h Target</span>
            </div>
            <div className="w-full h-2 bg-[#EFEFED] rounded-full overflow-hidden">
              <div className="h-full bg-[#202020] w-[72%]" />
            </div>
            <div className="flex items-center justify-between text-[11px] text-[#777773]">
              <span>5.4h committed</span>
              <span>2.1h focus buffer remaining</span>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};
