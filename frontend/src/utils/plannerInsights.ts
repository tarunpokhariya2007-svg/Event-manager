import { SchedulingResult } from '../types/planner';
import { Task } from '../types/task';
import { formatDuration, formatTime12h } from './timeUtils';

export type PlacementState = 'scheduled' | 'partial' | 'unscheduled';

export interface TaskPlacement {
  taskId: string;
  title: string;
  state: PlacementState;
  placedMinutes: number;
  sessions: number;
  remainingMinutes?: number;
  reason?: string;
  reasonCode?: string;
  suggestedAction?: string;
}

/** Combines the engine's timeline rows and unscheduled report into one placement state per task. */
export function computePlacements(result: SchedulingResult): Map<string, TaskPlacement> {
  const map = new Map<string, TaskPlacement>();
  for (const row of result.timeline) {
    if (row.type !== 'scheduled_task') continue;
    const p = map.get(row.referenceId) ?? { taskId: row.referenceId, title: row.title, state: 'scheduled' as PlacementState, placedMinutes: 0, sessions: 0 };
    p.placedMinutes += row.durationMinutes;
    p.sessions += 1;
    map.set(row.referenceId, p);
  }
  for (const u of result.unscheduledTasks) {
    const placed = map.get(u.task.id);
    map.set(u.task.id, {
      taskId: u.task.id,
      title: u.task.title,
      state: placed ? 'partial' : 'unscheduled',
      placedMinutes: placed?.placedMinutes ?? 0,
      sessions: placed?.sessions ?? 0,
      remainingMinutes: u.remainingMinutes,
      reason: u.reason,
      reasonCode: u.reasonCode,
      suggestedAction: u.suggestedAction,
    });
  }
  return map;
}

export const PLACEMENT_META: Record<PlacementState, { label: string; hint: string; style: string }> = {
  scheduled: { label: 'Scheduled', hint: 'All of this task’s estimated time was placed.', style: 'bg-[#F0FDF4] text-[#166534] border-[#BBF7D0]' },
  partial: { label: 'Partially scheduled', hint: 'Some sessions were placed, but part of the estimated time is still unplaced.', style: 'bg-[#FFFBEB] text-[#92400E] border-[#FDE68A]' },
  unscheduled: { label: 'Unscheduled', hint: 'None of this task could be placed.', style: 'bg-[#FEF2F2] text-[#991B1B] border-[#FECACA]' },
};

export const RISK_CATEGORY_META: Record<string, { label: string; hint: string }> = {
  deadline_risk: { label: 'Deadline risk', hint: 'The engine estimates this task finishing close to, or after, its deadline.' },
  hard_infeasible: { label: 'Deadline infeasible', hint: 'The engine found the deadline cannot be met with the current constraints.' },
  dependency_blocked: { label: 'Blocked by dependency', hint: 'A prerequisite task must be completed first.' },
  insufficient_capacity: { label: 'Not enough capacity', hint: 'There is not enough free time in the work window for this task.' },
};

export function humanizeCode(code?: string): string {
  return code ? code.replace(/_/g, ' ') : '';
}

export interface AppliedConstraint {
  label: string;
  value: string;
  detail: string;
  warn?: boolean;
}

/** Constraints that were actually sent to the engine, with real usage figures from the result. */
export function appliedConstraints(result: SchedulingResult): AppliedConstraint[] {
  const c = result.config;
  const cap = Math.round(c.maxDailyWorkload * 60);
  const atCap = cap > 0 && result.totalAllocatedMinutes >= cap;
  return [
    { label: 'Work window', value: `${formatTime12h(c.workHoursStart)} – ${formatTime12h(c.workHoursEnd)}`, detail: 'Tasks can only be placed inside this window.' },
    { label: 'Fixed events', value: `${result.fixedEvents.length}`, detail: 'Events on the target date that block their time range.' },
    { label: 'Minimum break', value: formatDuration(c.minBreakDuration), detail: 'Gap kept between placed blocks.' },
    {
      label: 'Daily workload cap',
      value: `${formatDuration(result.totalAllocatedMinutes)} of ${formatDuration(cap)}`,
      detail: atCap ? 'The cap is fully used, which can leave remaining tasks unplaced.' : 'Maximum task minutes the engine may allocate.',
      warn: atCap,
    },
    { label: 'Task splitting', value: c.allowTaskSplitting ? 'Allowed' : 'Off', detail: c.allowTaskSplitting ? 'Tasks may be split across free gaps (subject to each task’s own setting).' : 'Each task must fit in a single gap.' },
    { label: 'Objective', value: result.objective, detail: 'Strategy the engine used to rank and place tasks.' },
  ];
}

export interface PrerequisiteInfo {
  task: Task;
  completed: boolean;
  placement?: TaskPlacement;
}

/** Direct prerequisites of a task, with completion and (if available) placement state in a schedule. */
export function prerequisitesOf(task: Task, tasksById: Map<string, Task>, placements?: Map<string, TaskPlacement>): PrerequisiteInfo[] {
  const out: PrerequisiteInfo[] = [];
  for (const id of task.dependencies ?? []) {
    const dep = tasksById.get(id);
    if (!dep) continue;
    out.push({ task: dep, completed: dep.status === 'Completed', placement: placements?.get(id) });
  }
  return out;
}

/** Ids of tasks the schedule could not fully place because of dependencies (engine reports + unplaced prerequisites). */
export function computeBlockedIds(result: SchedulingResult, tasksById: Map<string, Task>, placements: Map<string, TaskPlacement>): Set<string> {
  const blocked = new Set<string>();
  for (const r of result.tasksAtRisk) if (r.category === 'dependency_blocked') blocked.add(r.task.id);
  for (const u of result.unscheduledTasks) {
    if (/depend/i.test(u.reasonCode ?? '')) blocked.add(u.task.id);
  }
  for (const p of placements.values()) {
    if (p.state === 'scheduled') continue;
    const task = tasksById.get(p.taskId);
    if (!task) continue;
    const unmet = prerequisitesOf(task, tasksById, placements).some(
      d => !d.completed && d.placement && d.placement.state !== 'scheduled'
    );
    if (unmet) blocked.add(p.taskId);
  }
  return blocked;
}
