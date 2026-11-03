import React, { useMemo } from 'react';
import { useApp } from '../../context/AppContext';
import { CheckSquare, Calendar, Clock, BarChart2, TrendingUp, AlertTriangle } from 'lucide-react';
import { MetricCard } from '../common/MetricCard';
import { formatDuration, getTodayDateString, parseMinutes } from '../../utils/timeUtils';
import { ScheduledTimelineItem } from '../../types/planner';

export const AnalyticsDashboard: React.FC = () => {
  const { tasks, events, schedulingResult } = useApp();

  const metrics = useMemo(() => {
    const total = tasks.length;
    const completed = tasks.filter(t => t.status === 'Completed').length;
    const pending = tasks.filter(t => t.status === 'Pending' || t.status === 'In Progress').length;
    const overdue = tasks.filter(t => t.status === 'Overdue').length;
    const completionRate = total > 0 ? Math.round((completed / total) * 100) : 0;

    // Hours actually allocated by the scheduler (no estimates)
    const allocatedMinutes = (schedulingResult?.timeline ?? [])
      .filter(i => i.type === 'scheduled_task')
      .reduce((sum, i) => sum + i.durationMinutes, 0);
    const totalScheduledHours = (allocatedMinutes / 60).toFixed(1);

    // Priorities
    const high = tasks.filter(t => t.priority === 'High').length;
    const medium = tasks.filter(t => t.priority === 'Medium').length;
    const low = tasks.filter(t => t.priority === 'Low').length;

    return {
      total,
      completed,
      pending,
      overdue,
      completionRate,
      totalScheduledHours,
      priorityCounts: { high, medium, low }
    };
  }, [tasks, schedulingResult]);

  const fmtH = (mins: number) => `${(mins / 60).toFixed(1)} Hours (${Math.round(mins)} mins)`;
  const dateOf = (iso?: string) => (iso ? iso.slice(0, 10) : '');

  // Current Mon–Sun week, from real task deadlines and completion timestamps
  const weeklyData = useMemo(() => {
    const [y, m, d] = getTodayDateString().split('-').map(Number);
    const base = new Date(y, m - 1, d);
    const monday = new Date(base);
    monday.setDate(base.getDate() + (base.getDay() === 0 ? -6 : 1 - base.getDay()));
    return Array.from({ length: 7 }, (_, i) => {
      const dt = new Date(monday);
      dt.setDate(monday.getDate() + i);
      const date = `${dt.getFullYear()}-${String(dt.getMonth() + 1).padStart(2, '0')}-${String(dt.getDate()).padStart(2, '0')}`;
      const planned = tasks.filter(t => dateOf(t.deadline) === date).length;
      const completed = tasks.filter(t => t.status === 'Completed' && dateOf(t.completedAt) === date).length;
      const scale = Math.max(planned, completed);
      return {
        day: dt.toLocaleDateString('en-US', { weekday: 'short' }),
        completed,
        planned,
        pct: scale > 0 ? Math.round((completed / scale) * 100) : 0
      };
    });
  }, [tasks]);
  const hasWeeklyData = weeklyData.some(w => w.planned > 0 || w.completed > 0);

  // Capacity + workload for the generated schedule's target day (clipped to the working window)
  const capacity = useMemo(() => {
    if (!schedulingResult) return null;
    const cfg = schedulingResult.config;
    const winStart = parseMinutes(cfg.workHoursStart);
    const winEnd = parseMinutes(cfg.workHoursEnd);
    const windowMins = Math.max(0, winEnd - winStart);
    const clipped = (i: ScheduledTimelineItem) =>
      Math.max(0, Math.min(parseMinutes(i.end), winEnd) - Math.max(parseMinutes(i.start), winStart));
    const sum = (type: ScheduledTimelineItem['type']) =>
      schedulingResult.timeline.filter(i => i.type === type).reduce((acc, i) => acc + clipped(i), 0);
    const eventMins = sum('fixed_event');
    const taskMins = sum('scheduled_task');
    const breakMins = sum('break');
    const freeMins = Math.max(0, windowMins - eventMins - taskMins - breakMins);

    const bucket = (from: number, to: number) =>
      schedulingResult.timeline
        .filter(i => i.type === 'scheduled_task')
        .reduce((acc, i) => acc + Math.max(0, Math.min(parseMinutes(i.end), to) - Math.max(parseMinutes(i.start), from)), 0);
    const windows = [
      { label: 'Morning (before 12:00)', mins: bucket(0, 720) },
      { label: 'Afternoon (12:00 – 17:00)', mins: bucket(720, 1020) },
      { label: 'Evening (after 17:00)', mins: bucket(1020, 1440) }
    ];
    return { windowMins, eventMins, taskMins, breakMins, freeMins, windows, date: schedulingResult.targetDate };
  }, [schedulingResult]);

  const highShare = metrics.total > 0 ? Math.round((metrics.priorityCounts.high / metrics.total) * 100) : 0;

  return (
    <div className="space-y-6">
      {/* 6 Key Metric Cards */}
      <div className="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-6 gap-3">
        <MetricCard label="Total Tasks" value={metrics.total} subtext="Tracked workload" />
        <MetricCard label="Completed" value={metrics.completed} subtext="Finished on schedule" />
        <MetricCard label="Pending" value={metrics.pending} subtext="Active backlog" />
        <MetricCard label="Overdue" value={metrics.overdue} subtext="Requires attention" alert={metrics.overdue > 0} />
        <MetricCard label="Completion Rate" value={`${metrics.completionRate}%`} subtext="Overall progress" />
        <MetricCard label="Scheduled Hours" value={`${metrics.totalScheduledHours}h`} subtext="Total workload" />
      </div>

      {/* Main Charts Row */}
      <div className="grid grid-cols-1 lg:grid-cols-12 gap-6">
        {/* Weekly Task Completion (Monochromatic Bar Chart) */}
        <div className="lg:col-span-7 bg-white border border-[#DEDEDA] rounded-lg p-5">
          <div className="flex items-center justify-between pb-3 border-b border-[#DEDEDA] mb-5">
            <div>
              <h3 className="text-sm font-semibold text-[#202020]">
                Weekly Task Completion Ratio
              </h3>
              <p className="text-xs text-[#777773] mt-0.5">
                Tasks completed vs. due (by deadline) across the current week.
              </p>
            </div>
            <div className="flex items-center gap-3 text-xs text-[#777773]">
              <div className="flex items-center gap-1.5">
                <span className="w-2.5 h-2.5 bg-[#202020] rounded-xs" />
                <span>Completed</span>
              </div>
              <div className="flex items-center gap-1.5">
                <span className="w-2.5 h-2.5 bg-[#DEDEDA] rounded-xs" />
                <span>Due</span>
              </div>
            </div>
          </div>

          {!hasWeeklyData && (
            <div className="h-56 flex items-center justify-center text-xs text-[#777773]">
              No tasks due or completed this week yet.
            </div>
          )}
          {hasWeeklyData && (
          <div className="h-56 flex items-end justify-between gap-4 pt-4 px-2">
            {weeklyData.map(item => (
              <div key={item.day} className="flex-1 flex flex-col items-center gap-2 h-full justify-end">
                <span className="text-[10px] font-mono text-[#777773]">
                  {item.completed}/{item.planned}
                </span>
                <div className="w-full max-w-[36px] bg-[#EFEFED] rounded-t flex flex-col justify-end h-40 p-0.5">
                  <div
                    className="w-full bg-[#202020] rounded-t transition-all duration-300"
                    style={{ height: `${item.pct}%` }}
                    title={`${item.day}: ${item.pct}% completed`}
                  />
                </div>
                <span className="text-xs font-medium text-[#202020]">{item.day}</span>
              </div>
            ))}
          </div>
          )}
        </div>

        {/* Task Distribution by Priority */}
        <div className="lg:col-span-5 bg-white border border-[#DEDEDA] rounded-lg p-5 flex flex-col justify-between">
          <div>
            <div className="pb-3 border-b border-[#DEDEDA] mb-4">
              <h3 className="text-sm font-semibold text-[#202020]">
                Task Distribution by Priority
              </h3>
              <p className="text-xs text-[#777773] mt-0.5">
                Workload allocation partitioned across priority tiers.
              </p>
            </div>

            <div className="space-y-4 pt-2 text-xs">
              {/* High Priority */}
              <div>
                <div className="flex items-center justify-between mb-1">
                  <span className="font-semibold text-[#202020]">High Priority</span>
                  <span className="font-mono text-[#555552]">{metrics.priorityCounts.high} tasks</span>
                </div>
                <div className="w-full h-2.5 bg-[#EFEFED] rounded-full overflow-hidden">
                  <div
                    className="h-full bg-[#202020]"
                    style={{
                      width: `${(metrics.priorityCounts.high / (metrics.total || 1)) * 100}%`
                    }}
                  />
                </div>
              </div>

              {/* Medium Priority */}
              <div>
                <div className="flex items-center justify-between mb-1">
                  <span className="font-semibold text-[#202020]">Medium Priority</span>
                  <span className="font-mono text-[#555552]">{metrics.priorityCounts.medium} tasks</span>
                </div>
                <div className="w-full h-2.5 bg-[#EFEFED] rounded-full overflow-hidden">
                  <div
                    className="h-full bg-[#777773]"
                    style={{
                      width: `${(metrics.priorityCounts.medium / (metrics.total || 1)) * 100}%`
                    }}
                  />
                </div>
              </div>

              {/* Low Priority */}
              <div>
                <div className="flex items-center justify-between mb-1">
                  <span className="font-semibold text-[#202020]">Low Priority</span>
                  <span className="font-mono text-[#555552]">{metrics.priorityCounts.low} tasks</span>
                </div>
                <div className="w-full h-2.5 bg-[#EFEFED] rounded-full overflow-hidden">
                  <div
                    className="h-full bg-[#DEDEDA]"
                    style={{
                      width: `${(metrics.priorityCounts.low / (metrics.total || 1)) * 100}%`
                    }}
                  />
                </div>
              </div>
            </div>
          </div>

          <div className="p-3 bg-[#FAFAF9] border border-[#DEDEDA] rounded-md text-[11px] text-[#777773] mt-5">
            <span className="font-semibold text-[#202020]">Scheduling Insight:</span>{' '}
            {metrics.total === 0
              ? 'Add tasks to see how your workload is distributed.'
              : `High-priority tasks make up ${highShare}% of your tasks (${metrics.priorityCounts.high} of ${metrics.total}).`}
          </div>
        </div>
      </div>

      {/* Secondary Charts Row: Scheduled vs. Available Hours & Daily Workload Distribution */}
      <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
        {/* Scheduled vs Available Hours */}
        <div className="bg-white border border-[#DEDEDA] rounded-lg p-5">
          <div className="pb-3 border-b border-[#DEDEDA] mb-4">
            <h3 className="text-sm font-semibold text-[#202020]">
              Scheduled vs. Available Hours{capacity ? ` (${capacity.date})` : ''}
            </h3>
            <p className="text-xs text-[#777773] mt-0.5">
              Capacity use within the configured working hours, from the latest generated schedule.
            </p>
          </div>

          {!capacity ? (
            <div className="py-8 text-center text-xs text-[#777773]">
              Generate a schedule in the Smart Planner to see capacity usage.
            </div>
          ) : (
          <div className="space-y-4 text-xs">
            <div className="flex items-center justify-between font-mono">
              <span className="text-[#777773]">Total Day Working Window:</span>
              <span className="font-semibold text-[#202020]">{fmtH(capacity.windowMins)}</span>
            </div>
            <div className="flex items-center justify-between font-mono">
              <span className="text-[#777773]">Fixed Event Commitments:</span>
              <span className="font-semibold text-[#202020]">{fmtH(capacity.eventMins)}</span>
            </div>
            <div className="flex items-center justify-between font-mono">
              <span className="text-[#777773]">Scheduled Task Time:</span>
              <span className="font-semibold text-[#202020]">{fmtH(capacity.taskMins)}</span>
            </div>
            <div className="flex items-center justify-between font-mono">
              <span className="text-[#777773]">Breaks:</span>
              <span className="font-semibold text-[#202020]">{fmtH(capacity.breakMins)}</span>
            </div>
            <div className="flex items-center justify-between font-mono border-t border-[#DEDEDA] pt-2">
              <span className="text-[#777773]">Available Hours:</span>
              <span className="font-semibold text-[#166534]">{fmtH(capacity.freeMins)}</span>
            </div>

            {capacity.windowMins > 0 && (
              <div className="w-full h-4 bg-[#EFEFED] rounded overflow-hidden flex mt-2">
                {[
                  { v: capacity.eventMins, c: 'bg-[#202020]', t: 'Fixed Events' },
                  { v: capacity.taskMins, c: 'bg-[#555552]', t: 'Scheduled Tasks' },
                  { v: capacity.breakMins, c: 'bg-[#888884]', t: 'Breaks' },
                  { v: capacity.freeMins, c: 'bg-[#DEDEDA]', t: 'Available' }
                ].map(seg => (
                  <div
                    key={seg.t}
                    style={{ width: `${(seg.v / capacity.windowMins) * 100}%` }}
                    className={seg.c}
                    title={`${seg.t}: ${Math.round((seg.v / capacity.windowMins) * 100)}%`}
                  />
                ))}
              </div>
            )}
          </div>
          )}
        </div>

        {/* Daily Workload Distribution */}
        <div className="bg-white border border-[#DEDEDA] rounded-lg p-5">
          <div className="pb-3 border-b border-[#DEDEDA] mb-4">
            <h3 className="text-sm font-semibold text-[#202020]">
              Daily Workload Distribution
            </h3>
            <p className="text-xs text-[#777773] mt-0.5">
              Generated task time across morning, afternoon, and evening.
            </p>
          </div>

          {!capacity || capacity.taskMins === 0 ? (
            <div className="py-8 text-center text-xs text-[#777773]">
              No scheduled task allocations yet.
            </div>
          ) : (
          <div className="space-y-3 text-xs">
            {capacity.windows.map(w => (
              <div key={w.label}>
                <div className="flex items-center justify-between text-[11px] mb-1">
                  <span className="font-medium text-[#202020]">{w.label}</span>
                  <span className="font-mono text-[#777773]">{(w.mins / 60).toFixed(1)} hrs</span>
                </div>
                <div className="w-full h-2 bg-[#EFEFED] rounded-full overflow-hidden">
                  <div className="h-full bg-[#202020]" style={{ width: `${(w.mins / capacity.taskMins) * 100}%` }} />
                </div>
              </div>
            ))}
          </div>
          )}
        </div>
      </div>
    </div>
  );
};
