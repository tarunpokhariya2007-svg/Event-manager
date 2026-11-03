import { CalendarEvent } from '../types/event';
import { Task } from '../types/task';
import {
  PlannerConfig,
  SchedulingResult,
  ScheduledTimelineItem,
  TaskAtRiskReport,
  TimeSlot,
  UnscheduledTaskReport,
} from '../types/planner';
import { SchedulingConflict } from '../types/conflict';
import { API_ENDPOINTS } from '../config/api';
import { ApiError, apiRequest } from './apiClient';
import {
  BackendId,
  fromBackendId,
  fromBackendPriority,
  hhmmToMinutes,
  toBackendEvent,
  toBackendId,
  toBackendTask,
} from './backendMappers';
import { alternativesFromSlots } from '../utils/conflictUtils';
import { parseMinutes } from '../utils/timeUtils';

/** Request body of POST /api/v1/schedule/optimize (backend API_CONTRACT.md §4/§5). */
export interface CXXOptimizationPayload {
  version: string;
  data_source?: 'request' | 'database';
  config: {
    target_date: string;
    end_date?: string;
    work_window_start_min: number;
    work_window_end_min: number;
    min_break_min: number;
    allow_splitting: boolean;
    max_workload_min: number;
    objective: 'balanced' | 'priority' | 'deadline' | 'compact' | 'edf';
  };
  fixed_events: Array<Record<string, unknown>>;
  flexible_tasks: Array<Record<string, unknown>>;
}

interface BackendResult {
  target_date: string;
  status: 'success' | 'partial' | 'infeasible';
  solver_duration_ms: number;
  total_allocated_min: number;
  total_available_min: number;
  objective?: string;
  statistics?: Record<string, unknown>;
  warnings?: string[];
  validation_errors?: Array<{ message: string }>;
  timeline: Array<{
    id: string;
    type: 'event' | 'task';
    reference_id: BackendId;
    title: string;
    start: string;
    end: string;
    date?: string;
    duration_min: number;
    priority: number;
    status: 'fixed' | 'scheduled';
    explanation: string;
    session_index: number | null;
    session_count: number | null;
  }>;
  available_slots: Array<{ id: string; date: string; start: string; end: string; duration_min: number; label: string }>;
  unscheduled_tasks: Array<{
    task_id: BackendId;
    title: string;
    reason: string;
    suggested_action: string;
    remaining_min?: number;
    reason_code?: string;
  }>;
  tasks_at_risk: Array<{
    task_id: BackendId;
    title: string;
    risk_level: 'on_track' | 'at_risk' | 'impossible_without_rescheduling';
    buffer_min: number;
    reason: string;
    category?: string;
    deadline?: string | null;
    estimated_completion?: string | null;
  }>;
  conflicts: Array<{
    id: string;
    event_a_id: BackendId;
    event_b_id: BackendId;
    date: string;
    overlap_start: string;
    overlap_end: string;
    message: string;
  }>;
}

const OBJECTIVE_MAP: Record<PlannerConfig['optimizationGoal'], CXXOptimizationPayload['config']['objective']> = {
  balanced: 'balanced',
  deadline_urgency: 'deadline',
  context_clustering: 'compact',
};

export class CXXSchedulerClient {
  /** GET /api/v1/schedule/optimize/health */
  async ping(): Promise<{ online: boolean; latencyMs: number; engineVersion?: string; error?: string }> {
    const start = performance.now();
    try {
      const res = await apiRequest<{ status?: string; engine_version?: string }>(API_ENDPOINTS.health, { timeoutMs: 3000 });
      return {
        online: res?.status === 'online',
        latencyMs: Math.round(performance.now() - start),
        engineVersion: res?.engine_version,
      };
    } catch (e) {
      return { online: false, latencyMs: 0, error: e instanceof Error ? e.message : 'Backend unreachable.' };
    }
  }

  buildPayload(config: PlannerConfig, events: CalendarEvent[], tasks: Task[]): CXXOptimizationPayload {
    const dayEvents = events.filter(e => e.date === config.date);
    const sentTasks = config.includePendingTasks ? tasks : [];
    const knownIds = new Set(sentTasks.map(t => t.id));

    return {
      version: '1.0',
      data_source: 'request',
      config: {
        target_date: config.date,
        end_date: config.date,
        work_window_start_min: parseMinutes(config.workHoursStart),
        work_window_end_min: parseMinutes(config.workHoursEnd),
        min_break_min: Math.max(0, Math.round(config.minBreakDuration)),
        allow_splitting: config.allowTaskSplitting,
        max_workload_min: Math.min(1440, Math.max(1, Math.round(config.maxDailyWorkload * 60))),
        objective: OBJECTIVE_MAP[config.optimizationGoal] ?? 'balanced',
      },
      fixed_events: dayEvents.map(e => ({ id: toBackendId(e.id), ...toBackendEvent(e) })),
      flexible_tasks: sentTasks.map(t => {
        const body = toBackendTask({ ...t, dependencies: t.dependencies.filter(d => knownIds.has(d)) });
        return { id: toBackendId(t.id), ...body };
      }),
    };
  }

  /** POST /api/v1/schedule/optimize — real C++ scheduling. Throws ApiError; never returns simulated data. */
  async generateSchedule(config: PlannerConfig, events: CalendarEvent[], tasks: Task[]): Promise<SchedulingResult> {
    const payload = this.buildPayload(config, events, tasks);
    const res = await apiRequest<BackendResult>(API_ENDPOINTS.optimize, { method: 'POST', body: payload });

    if (!res || !Array.isArray(res.timeline) || !Array.isArray(res.available_slots) || typeof res.status !== 'string') {
      throw new ApiError('invalid_response', 'The backend returned an unexpected scheduling response.');
    }
    return this.mapResult(res, config, events, tasks);
  }

  private mapResult(res: BackendResult, config: PlannerConfig, events: CalendarEvent[], tasks: Task[]): SchedulingResult {
    const eventById = new Map(events.map(e => [e.id, e]));
    const taskById = new Map(tasks.map(t => [t.id, t]));

    const timeline: ScheduledTimelineItem[] = res.timeline.map(row => {
      const refId = fromBackendId(row.reference_id);
      const isEvent = row.type === 'event';
      const isBreak = (row.type as string) === 'break';
      const evt = isEvent ? eventById.get(refId) : undefined;
      const task = !isEvent && !isBreak ? taskById.get(refId) : undefined;
      const sessions = row.session_count ?? 1;
      return {
        id: row.id,
        type: isEvent ? 'fixed_event' : isBreak ? 'break' : 'scheduled_task',
        title: row.title,
        start: row.start,
        end: row.end,
        date: row.date,
        durationMinutes: row.duration_min,
        priority: fromBackendPriority(row.priority),
        status: row.status === 'fixed' ? 'Fixed Event' : isBreak ? 'Break' : 'Scheduled Task',
        referenceId: refId,
        explanation: row.explanation,
        isSplitPart: !isEvent && !isBreak && sessions > 1,
        splitIndex: row.session_index ?? undefined,
        totalSplits: row.session_count ?? undefined,
        category: evt?.category ?? task?.category,
        location: evt?.location,
      };
    });

    const availableSlots: TimeSlot[] = res.available_slots.map(s => ({
      id: s.id,
      start: s.start,
      end: s.end,
      durationMinutes: s.duration_min,
      isAvailable: true,
      label: s.label,
    }));

    const stubTask = (id: string, title: string, minutes: number): Task => ({
      id,
      title,
      estimatedDuration: minutes,
      priority: 'Medium',
      deadline: '',
      allowSplitting: true,
      dependencies: [],
      status: 'Pending',
    });

    const unscheduledTasks: UnscheduledTaskReport[] = (res.unscheduled_tasks ?? []).map(u => {
      const id = fromBackendId(u.task_id);
      return {
        task: taskById.get(id) ?? stubTask(id, u.title, u.remaining_min ?? 0),
        reason: u.reason,
        suggestedAction: u.suggested_action,
        remainingMinutes: u.remaining_min,
        reasonCode: u.reason_code,
      };
    });

    const tasksAtRisk: TaskAtRiskReport[] = (res.tasks_at_risk ?? []).map(r => {
      const id = fromBackendId(r.task_id);
      return {
        task: taskById.get(id) ?? stubTask(id, r.title, 0),
        reason: r.reason,
        riskLevel: r.risk_level === 'impossible_without_rescheduling' ? 'High' : r.risk_level === 'at_risk' ? 'Medium' : 'Low',
        bufferMinutes: r.buffer_min,
        category: r.category,
        deadline: r.deadline ?? null,
        estimatedCompletion: r.estimated_completion ?? null,
      };
    });

    const conflicts: SchedulingConflict[] = [];
    for (const c of res.conflicts ?? []) {
      const a = eventById.get(fromBackendId(c.event_a_id));
      const b = eventById.get(fromBackendId(c.event_b_id));
      if (!a || !b) continue; // event no longer known to the UI
      const durationB = parseMinutes(b.endTime) - parseMinutes(b.startTime);
      conflicts.push({
        id: `conflict-${a.id}-${b.id}`,
        eventA: a,
        eventB: b,
        date: c.date,
        overlapStart: c.overlap_start,
        overlapEnd: c.overlap_end,
        description: c.message,
        suggestedAlternatives: alternativesFromSlots(
          res.available_slots.map(s => ({ start: s.start, end: s.end, date: s.date })),
          durationB,
          events.filter(e => e.date === c.date && e.id !== b.id)
        ),
        status: 'unresolved',
      });
    }

    const statistics: Record<string, number> = {};
    for (const [k, v] of Object.entries(res.statistics ?? {})) {
      if (typeof v === 'number') statistics[k] = v;
    }

    const placedTaskIds = new Set(timeline.filter(t => t.type === 'scheduled_task').map(t => t.referenceId));
    const inTimeline = new Set(timeline.filter(t => t.type === 'fixed_event').map(t => t.referenceId));

    return {
      generatedAt: new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' }),
      generatedAtIso: new Date().toISOString(),
      config: { ...config },
      targetDate: res.target_date ?? config.date,
      timeline,
      availableSlots,
      fixedEvents: events.filter(e => inTimeline.has(e.id)),
      allocatedTasksCount: placedTaskIds.size,
      unscheduledTasks,
      tasksAtRisk,
      conflicts,
      totalAllocatedMinutes: res.total_allocated_min,
      totalAvailableMinutes: res.total_available_min,
      engineMode: 'cxx_connected',
      status: res.status,
      warnings: res.warnings ?? [],
      statistics,
      solverDurationMs: res.solver_duration_ms,
      objective: res.objective ?? OBJECTIVE_MAP[config.optimizationGoal],
    };
  }
}

export const cxxSchedulerService = new CXXSchedulerClient();
