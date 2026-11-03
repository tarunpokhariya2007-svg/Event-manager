import { CalendarEvent } from './event';

export interface AlternativeTimeSuggestion {
  id: string;
  startTime: string; // HH:mm
  endTime: string;   // HH:mm
  date: string;      // YYYY-MM-DD
  label: string;     // e.g. "01:00 PM – 02:00 PM"
  rationale: string; // e.g. "Clean 1hr gap between DSA Assignment and DBMS Revision"
  recommended?: boolean;
}

export interface SchedulingConflict {
  id: string;
  eventA: CalendarEvent;
  eventB: CalendarEvent;
  date: string;
  overlapStart: string; // HH:mm
  overlapEnd: string;   // HH:mm
  description: string;
  suggestedAlternatives: AlternativeTimeSuggestion[];
  status: 'unresolved' | 'resolved';
  resolvedAt?: string;
  selectedAlternative?: AlternativeTimeSuggestion;
}
