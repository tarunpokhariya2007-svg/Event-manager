import { ScheduleHistoryEntry, SchedulingResult } from '../types/planner';

const KEY = 'chronoplan_schedule_history_v1';
export const MAX_HISTORY = 10;

function persist(entries: ScheduleHistoryEntry[]): ScheduleHistoryEntry[] {
  try {
    localStorage.setItem(KEY, JSON.stringify(entries));
  } catch (e) {
    console.warn('Failed to save schedule history', e);
  }
  return entries;
}

/**
 * Lightweight, browser-local history of generated schedules. The C++ backend does not persist
 * schedules (the optimize endpoint is stateless), so history lives in localStorage only.
 */
export const scheduleHistoryService = {
  load(): ScheduleHistoryEntry[] {
    try {
      const raw = localStorage.getItem(KEY);
      const parsed = raw ? JSON.parse(raw) : [];
      // Drop entries written by older builds that lack the fields the UI relies on.
      return Array.isArray(parsed) ? parsed.filter(e => e?.id && e?.result?.config && e?.result?.timeline) : [];
    } catch {
      return [];
    }
  },

  add(result: SchedulingResult): ScheduleHistoryEntry[] {
    const entry: ScheduleHistoryEntry = {
      id: `hist-${Date.now()}-${Math.random().toString(36).slice(2, 6)}`,
      savedAt: result.generatedAtIso,
      result,
    };
    return persist([entry, ...this.load()].slice(0, MAX_HISTORY));
  },

  remove(id: string): ScheduleHistoryEntry[] {
    return persist(this.load().filter(e => e.id !== id));
  },

  clear(): ScheduleHistoryEntry[] {
    return persist([]);
  },
};
