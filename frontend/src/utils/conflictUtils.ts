import { CalendarEvent } from '../types/event';
import { AlternativeTimeSuggestion, SchedulingConflict } from '../types/conflict';
import { formatDuration, formatTime12h, parseMinutes } from './timeUtils';
import { minutesToHHmm } from '../services/backendMappers';

export interface FreeSlot {
  start: string; // HH:mm
  end: string;   // HH:mm
  date: string;  // YYYY-MM-DD
}

/** Free gaps inside the work window that no event occupies (computed from real events). */
export function freeSlotsForDay(
  date: string,
  events: CalendarEvent[],
  workStart: string,
  workEnd: string
): FreeSlot[] {
  const winStart = parseMinutes(workStart);
  const winEnd = parseMinutes(workEnd);
  const busy = events
    .filter(e => e.date === date)
    .map(e => [parseMinutes(e.startTime), parseMinutes(e.endTime)] as const)
    .sort((a, b) => a[0] - b[0]);

  const slots: FreeSlot[] = [];
  let cursor = winStart;
  for (const [s, e] of busy) {
    if (s > cursor) slots.push({ start: minutesToHHmm(cursor), end: minutesToHHmm(Math.min(s, winEnd)), date });
    cursor = Math.max(cursor, e);
    if (cursor >= winEnd) break;
  }
  if (cursor < winEnd) slots.push({ start: minutesToHHmm(cursor), end: minutesToHHmm(winEnd), date });
  return slots.filter(s => parseMinutes(s.end) > parseMinutes(s.start));
}

/** Why a free slot is available: names the neighbouring events (from real calendar data) that bound it. */
function explainSlot(slot: FreeSlot, durationMin: number, events: CalendarEvent[]): string {
  const a = parseMinutes(slot.start);
  const b = parseMinutes(slot.end);
  const day = events.filter(e => e.date === slot.date);
  const prev = day.filter(e => parseMinutes(e.endTime) <= a).sort((x, y) => parseMinutes(y.endTime) - parseMinutes(x.endTime))[0];
  const next = day.filter(e => parseMinutes(e.startTime) >= b).sort((x, y) => parseMinutes(x.startTime) - parseMinutes(y.startTime))[0];
  const from = prev ? `after "${prev.title}" ends at ${formatTime12h(prev.endTime)}` : 'from the start of your work window';
  const to = next ? `until "${next.title}" starts at ${formatTime12h(next.startTime)}` : 'to the end of your work window';
  const use = b - a > durationMin ? ` Uses the first ${formatDuration(durationMin)} of it.` : '';
  return `No event overlaps this ${formatDuration(b - a)} gap (${from} ${to}).${use}`;
}

/** Up to three alternatives: the first `durationMin` minutes of each free slot that is long enough. */
export function alternativesFromSlots(slots: FreeSlot[], durationMin: number, events: CalendarEvent[] = []): AlternativeTimeSuggestion[] {
  const out: AlternativeTimeSuggestion[] = [];
  for (const slot of slots) {
    const a = parseMinutes(slot.start);
    const b = parseMinutes(slot.end);
    if (durationMin <= 0 || b - a < durationMin) continue;
    const end = minutesToHHmm(a + durationMin);
    out.push({
      id: `alt-${out.length + 1}`,
      startTime: slot.start,
      endTime: end,
      date: slot.date,
      label: `${formatTime12h(slot.start)} – ${formatTime12h(end)}`,
      rationale: explainSlot(slot, durationMin, events),
      recommended: out.length === 0,
    });
    if (out.length === 3) break;
  }
  return out;
}

/** The event that would be moved to resolve a conflict: the flexible one (B first); otherwise A. */
export function pickMovableEvent(a: CalendarEvent, b: CalendarEvent): CalendarEvent {
  if (b.flexibility === 'Flexible') return b;
  if (a.flexibility === 'Flexible') return a;
  return a;
}

export type ConflictSeverity = 'high' | 'medium' | 'low';

export interface ConflictClassification {
  severity: ConflictSeverity;
  label: string;
  reason: string;
  overlapMinutes: number;
  overlapPercent: number; // of the shorter event
  movableEventId: string;
  bothFixed: boolean;
}

/** Rule-based classification using only event data: flexibility and how much of the shorter event overlaps. */
export function classifyConflict(c: SchedulingConflict): ConflictClassification {
  const { eventA: a, eventB: b } = c;
  const overlapMinutes = Math.max(0, parseMinutes(c.overlapEnd) - parseMinutes(c.overlapStart));
  const shorter = Math.max(1, Math.min(parseMinutes(a.endTime) - parseMinutes(a.startTime), parseMinutes(b.endTime) - parseMinutes(b.startTime)));
  const overlapPercent = Math.min(100, Math.round((overlapMinutes / shorter) * 100));
  const bothFixed = a.flexibility === 'Fixed' && b.flexibility === 'Fixed';
  const movableEventId = pickMovableEvent(a, b).id;

  if (bothFixed) {
    return { severity: 'high', label: 'Hard clash', reason: 'Both events are Fixed, so neither can be moved automatically.', overlapMinutes, overlapPercent, movableEventId, bothFixed };
  }
  if (overlapPercent >= 50) {
    return { severity: 'medium', label: 'Major overlap', reason: `${overlapPercent}% of the shorter event overlaps; one event is flexible and can be moved.`, overlapMinutes, overlapPercent, movableEventId, bothFixed };
  }
  return { severity: 'low', label: 'Minor overlap', reason: `${overlapPercent}% of the shorter event overlaps; one event is flexible and can be moved.`, overlapMinutes, overlapPercent, movableEventId, bothFixed };
}
