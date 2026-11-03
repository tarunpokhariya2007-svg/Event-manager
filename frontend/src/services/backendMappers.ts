import { Task, TaskPriority, TaskStatus } from '../types/task';
import { CalendarEvent, EventCategory, EventPriority } from '../types/event';
import { parseMinutes } from '../utils/timeUtils';

/**
 * Mapping between frontend models and the C++ backend's JSON (see backend API_CONTRACT.md).
 *
 * IDs: the backend keeps the JSON type of an id (7 and "7" are different ids). The frontend uses
 * string ids, so a numeric backend id n is represented as "#n" (never a valid backend string id).
 */
export type BackendId = string | number;

export function fromBackendId(id: BackendId): string {
  return typeof id === 'number' ? `#${id}` : String(id);
}

export function toBackendId(id: string): BackendId {
  if (/^#(0|[1-9]\d*)$/.test(id)) return Number(id.slice(1));
  return id;
}

/** URL path segment for an id (API_CONTRACT.md §1.1). */
export function idToPath(id: string): string {
  const b = toBackendId(id);
  if (typeof b === 'number') return String(b);
  if (/^(0|[1-9]\d*)$/.test(b)) return encodeURIComponent(`"${b}"`); // string id that looks numeric
  return encodeURIComponent(b);
}

// ---- priority: frontend High/Medium/Low <-> backend 1..5 (5 = highest) ----
export function toBackendPriority(p: TaskPriority | EventPriority): number {
  return p === 'High' ? 5 : p === 'Low' ? 1 : 3;
}

export function fromBackendPriority(n: number | null | undefined): TaskPriority {
  if (typeof n !== 'number') return 'Medium';
  return n >= 4 ? 'High' : n <= 2 ? 'Low' : 'Medium';
}

// ---- time helpers ----
/** Minutes since midnight -> "HH:mm". 1440 (end of day) is "24:00", as in the backend's responses. */
export function minutesToHHmm(min: number): string {
  const m = Math.min(Math.max(Math.round(min), 0), 1440);
  return `${String(Math.floor(m / 60)).padStart(2, '0')}:${String(m % 60).padStart(2, '0')}`;
}

/**
 * Task deadlines are "YYYY-MM-DDTHH:mm" strings that feed datetime-local inputs and Date parsing,
 * where "24:00" is invalid. The backend's end-of-day deadline (1440) is therefore shown as 23:59,
 * and 23:59 is sent back as 1440 so an end-of-day deadline does not drift by a minute when saved.
 */
function deadlineTime(min: number): string {
  return min >= 1440 ? '23:59' : minutesToHHmm(min);
}

/** Backend response clock strings may be "24:00" (end of day). */
export function hhmmToMinutes(s: string): number {
  return parseMinutes(s);
}

function splitDateTime(value: string | undefined): { date: string; min: number | null } | null {
  if (!value) return null;
  const m = /^(\d{4}-\d{2}-\d{2})(?:[T ](\d{2}):(\d{2}))?/.exec(value);
  if (!m) return null;
  return { date: m[1], min: m[2] !== undefined ? Number(m[2]) * 60 + Number(m[3]) : null };
}

// ---- tasks ----
export interface BackendTask {
  id: BackendId | null;
  title: string;
  description: string | null;
  technical_context?: string | null;
  priority: number;
  duration_min: number;
  min_chunk_min?: number;
  allow_split: boolean;
  deadline_date: string | null;
  deadline_min: number;
  earliest_start_min: number | null;
  latest_completion_min: number | null;
  prerequisite_ids: BackendId[];
  status: string | null;
}

/** Frontend-only fields the backend has no column for; kept in localStorage per record id. */
export interface TaskExtras {
  category?: string;
  completedAt?: string;
  earliestStartDate?: string;
  latestAcceptableCompletionDate?: string;
}
export interface EventExtras {
  description?: string;
  isRecurring?: boolean;
}

export function toBackendTask(task: Omit<Task, 'id'> & { id?: string }): Record<string, unknown> {
  const dl = splitDateTime(task.deadline);
  const es = splitDateTime(task.earliestStartDate);
  const lc = splitDateTime(task.latestAcceptableCompletionDate);
  // The backend applies these as times of day on every day. Each constraint is sent on its own
  // when supplied; the ordering rule only applies when both are given.
  const earliest = es && es.min !== null ? es.min : null; // 0..1439
  const latestRaw = lc && lc.min !== null ? lc.min : null;
  const latest = latestRaw !== null && latestRaw >= 1 ? latestRaw : null; // backend range is 1..1440
  if (earliest !== null && latest !== null && latest <= earliest) {
    throw new Error(
      `"${task.title}": the latest completion time of day must be after the earliest start time of day (the backend applies both as daily times).`
    );
  }

  return {
    title: task.title,
    description: task.description ? task.description : null,
    priority: toBackendPriority(task.priority),
    duration_min: task.estimatedDuration,
    allow_split: task.allowSplitting,
    deadline_date: dl ? dl.date : null,
    deadline_min: dl && dl.min !== null ? (dl.min === 1439 ? 1440 : dl.min) : 1440,
    earliest_start_min: earliest,
    latest_completion_min: latest,
    prerequisite_ids: task.dependencies.map(toBackendId),
    status: task.status,
  };
}

const TASK_STATUSES: TaskStatus[] = ['Pending', 'In Progress', 'Completed', 'Overdue', 'Blocked'];

function normalizeStatus(s: string | null | undefined): TaskStatus {
  if (!s) return 'Pending';
  const hit = TASK_STATUSES.find(t => t.toLowerCase() === s.toLowerCase());
  if (hit) return hit;
  if (['done', 'complete', 'finished'].includes(s.toLowerCase())) return 'Completed';
  return 'Pending';
}

export function fromBackendTask(bt: BackendTask, extras: TaskExtras = {}): Task {
  return {
    id: fromBackendId(bt.id as BackendId),
    title: bt.title,
    description: bt.description ?? undefined,
    estimatedDuration: bt.duration_min,
    priority: fromBackendPriority(bt.priority),
    deadline: bt.deadline_date ? `${bt.deadline_date}T${deadlineTime(bt.deadline_min)}` : '',
    earliestStartDate: extras.earliestStartDate,
    latestAcceptableCompletionDate: extras.latestAcceptableCompletionDate,
    allowSplitting: bt.allow_split,
    dependencies: (bt.prerequisite_ids ?? []).map(fromBackendId),
    status: normalizeStatus(bt.status),
    completedAt: extras.completedAt,
    category: extras.category,
  };
}

export function taskExtrasOf(task: Task): TaskExtras {
  return {
    category: task.category,
    completedAt: task.completedAt,
    earliestStartDate: task.earliestStartDate,
    latestAcceptableCompletionDate: task.latestAcceptableCompletionDate,
  };
}

// ---- events ----
export interface BackendEvent {
  id: BackendId | null;
  title: string;
  date: string;
  start_min: number;
  end_min: number;
  priority: number;
  is_flexible: boolean;
  category: string;
  location: string;
}

const EVENT_CATEGORIES: EventCategory[] = ['Academic', 'Work', 'Meeting', 'Lecture', 'Personal', 'Milestone'];

export function toBackendEvent(evt: Omit<CalendarEvent, 'id'>): Record<string, unknown> {
  return {
    title: evt.title,
    date: evt.date,
    start_min: parseMinutes(evt.startTime),
    end_min: parseMinutes(evt.endTime),
    priority: toBackendPriority(evt.priority),
    is_flexible: evt.flexibility === 'Flexible',
    category: evt.category,
    location: evt.location ?? '',
  };
}

export function fromBackendEvent(be: BackendEvent, extras: EventExtras = {}): CalendarEvent {
  const category = EVENT_CATEGORIES.find(c => c.toLowerCase() === (be.category ?? '').toLowerCase()) ?? 'Personal';
  return {
    id: fromBackendId(be.id as BackendId),
    title: be.title,
    description: extras.description,
    date: be.date,
    startTime: minutesToHHmm(be.start_min),
    endTime: minutesToHHmm(be.end_min),
    location: be.location ? be.location : undefined,
    category,
    priority: fromBackendPriority(be.priority),
    flexibility: be.is_flexible ? 'Flexible' : 'Fixed',
    isRecurring: extras.isRecurring,
  };
}

export function eventExtrasOf(evt: CalendarEvent): EventExtras {
  return { description: evt.description, isRecurring: evt.isRecurring };
}
