import { Task } from '../types/task';
import { CalendarEvent } from '../types/event';
import { API_ENDPOINTS } from '../config/api';
import { apiRequest } from './apiClient';
import {
  BackendEvent,
  BackendTask,
  EventExtras,
  TaskExtras,
  eventExtrasOf,
  fromBackendEvent,
  fromBackendTask,
  idToPath,
  taskExtrasOf,
  toBackendEvent,
  toBackendTask,
} from './backendMappers';

/** Frontend-only metadata (fields the backend has no column for), keyed by record id. */
const TASK_EXTRAS_KEY = 'chronoplan_task_extras_v1';
const EVENT_EXTRAS_KEY = 'chronoplan_event_extras_v1';

function loadExtras<T>(key: string): Record<string, T> {
  try {
    const raw = localStorage.getItem(key);
    if (raw) return JSON.parse(raw) as Record<string, T>;
  } catch {
    /* ignore corrupt metadata */
  }
  return {};
}

function saveExtras<T>(key: string, value: Record<string, T>): void {
  try {
    localStorage.setItem(key, JSON.stringify(value));
  } catch {
    /* metadata is best-effort */
  }
}

function putExtras<T>(key: string, id: string, extras: T): void {
  const all = loadExtras<T>(key);
  all[id] = extras;
  saveExtras(key, all);
}

function dropExtras(key: string, id: string): void {
  const all = loadExtras<unknown>(key);
  delete all[id];
  saveExtras(key, all);
}

function pruneExtras(key: string, liveIds: string[]): void {
  const all = loadExtras<unknown>(key);
  const live = new Set(liveIds);
  let changed = false;
  for (const id of Object.keys(all)) {
    if (!live.has(id)) {
      delete all[id];
      changed = true;
    }
  }
  if (changed) saveExtras(key, all);
}

export function clearLocalExtras(): void {
  localStorage.removeItem(TASK_EXTRAS_KEY);
  localStorage.removeItem(EVENT_EXTRAS_KEY);
}

export const backendDataService = {
  // ---- tasks: GET/POST /tasks, PUT/DELETE /tasks/<id> ----
  async listTasks(): Promise<Task[]> {
    const rows = await apiRequest<BackendTask[]>(API_ENDPOINTS.tasks);
    const extras = loadExtras<TaskExtras>(TASK_EXTRAS_KEY);
    const tasks = rows.map(r => fromBackendTask(r, extras[`#${r.id}`] ?? extras[String(r.id)]));
    pruneExtras(TASK_EXTRAS_KEY, tasks.map(t => t.id));
    return tasks.reverse(); // newest first, as the UI showed before
  },

  async createTask(task: Omit<Task, 'id'>): Promise<Task> {
    const row = await apiRequest<BackendTask>(API_ENDPOINTS.tasks, { method: 'POST', body: toBackendTask(task) });
    const created = fromBackendTask(row, taskExtrasOf(task as Task));
    putExtras(TASK_EXTRAS_KEY, created.id, taskExtrasOf(created));
    return created;
  },

  async updateTask(task: Task): Promise<Task> {
    // PUT is a full replacement on the backend: the complete task is always sent.
    const row = await apiRequest<BackendTask>(`${API_ENDPOINTS.tasks}/${idToPath(task.id)}`, {
      method: 'PUT',
      body: toBackendTask(task),
    });
    const updated = fromBackendTask(row, taskExtrasOf(task));
    putExtras(TASK_EXTRAS_KEY, updated.id, taskExtrasOf(updated));
    return updated;
  },

  async deleteTask(id: string): Promise<void> {
    await apiRequest<void>(`${API_ENDPOINTS.tasks}/${idToPath(id)}`, { method: 'DELETE' });
    dropExtras(TASK_EXTRAS_KEY, id);
  },

  // ---- events: GET/POST /events, PUT/DELETE /events/<id> ----
  async listEvents(): Promise<CalendarEvent[]> {
    const rows = await apiRequest<BackendEvent[]>(API_ENDPOINTS.events);
    const extras = loadExtras<EventExtras>(EVENT_EXTRAS_KEY);
    const events = rows.map(r => fromBackendEvent(r, extras[`#${r.id}`] ?? extras[String(r.id)]));
    pruneExtras(EVENT_EXTRAS_KEY, events.map(e => e.id));
    return events;
  },

  async createEvent(evt: Omit<CalendarEvent, 'id'>): Promise<CalendarEvent> {
    const row = await apiRequest<BackendEvent>(API_ENDPOINTS.events, { method: 'POST', body: toBackendEvent(evt) });
    const created = fromBackendEvent(row, eventExtrasOf(evt as CalendarEvent));
    putExtras(EVENT_EXTRAS_KEY, created.id, eventExtrasOf(created));
    return created;
  },

  async updateEvent(evt: CalendarEvent): Promise<CalendarEvent> {
    const row = await apiRequest<BackendEvent>(`${API_ENDPOINTS.events}/${idToPath(evt.id)}`, {
      method: 'PUT',
      body: toBackendEvent(evt),
    });
    const updated = fromBackendEvent(row, eventExtrasOf(evt));
    putExtras(EVENT_EXTRAS_KEY, updated.id, eventExtrasOf(updated));
    return updated;
  },

  async deleteEvent(id: string): Promise<void> {
    await apiRequest<void>(`${API_ENDPOINTS.events}/${idToPath(id)}`, { method: 'DELETE' });
    dropExtras(EVENT_EXTRAS_KEY, id);
  },
};
