import { CalendarEvent } from './event';
import { Task, TaskPriority } from './task';
import { SchedulingConflict } from './conflict';

export interface PlannerConfig {
  date: string; // YYYY-MM-DD
  workHoursStart: string; // HH:mm e.g. "08:00"
  workHoursEnd: string;   // HH:mm e.g. "18:00"
  minBreakDuration: number; // in minutes e.g. 30
  includePendingTasks: boolean;
  allowTaskSplitting: boolean;
  maxDailyWorkload: number; // in hours e.g. 7
  optimizationGoal: 'balanced' | 'deadline_urgency' | 'context_clustering';
}

export interface TimeSlot {
  id: string;
  start: string; // HH:mm
  end: string;   // HH:mm
  durationMinutes: number;
  isAvailable: boolean;
  label?: string;
}

export type TimelineItemType = 'fixed_event' | 'scheduled_task' | 'break';

export interface ScheduledTimelineItem {
  id: string;
  type: TimelineItemType;
  title: string;
  start: string; // HH:mm
  end: string;   // HH:mm
  durationMinutes: number;
  priority: TaskPriority;
  status: string;
  referenceId: string;
  explanation?: string;
  isSplitPart?: boolean;
  splitIndex?: number;
  totalSplits?: number;
  category?: string;
  location?: string;
  date?: string; // YYYY-MM-DD (from the backend row)
}

export interface UnscheduledTaskReport {
  task: Task;
  reason: string;
  suggestedAction: string;
  remainingMinutes?: number;
  reasonCode?: string;
}

export interface TaskAtRiskReport {
  task: Task;
  reason: string;
  riskLevel: 'High' | 'Medium' | 'Low';
  bufferMinutes: number;
  category?: string;            // deadline_risk | hard_infeasible | dependency_blocked | insufficient_capacity
  deadline?: string | null;     // YYYY-MM-DDTHH:mm
  estimatedCompletion?: string | null;
}

export interface SchedulingResult {
  generatedAt: string;
  /** ISO timestamp of generation (used by the local schedule history). */
  generatedAtIso: string;
  /** Snapshot of the planner settings that were sent to the engine. */
  config: PlannerConfig;
  targetDate: string;
  timeline: ScheduledTimelineItem[];
  availableSlots: TimeSlot[];
  fixedEvents: CalendarEvent[];
  allocatedTasksCount: number;
  unscheduledTasks: UnscheduledTaskReport[];
  tasksAtRisk: TaskAtRiskReport[];
  conflicts: SchedulingConflict[];
  totalAllocatedMinutes: number;
  totalAvailableMinutes: number;
  /** Always 'cxx_connected': a result is only built from a real response of the C++ backend. */
  engineMode: 'cxx_connected';
  status: 'success' | 'partial' | 'infeasible';
  warnings: string[];
  statistics: Record<string, number>;
  solverDurationMs: number;
  objective: string;
}

/** A previously generated schedule kept in this browser (the backend does not persist schedules). */
export interface ScheduleHistoryEntry {
  id: string;
  savedAt: string; // ISO
  result: SchedulingResult;
}
