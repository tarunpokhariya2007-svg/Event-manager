// Utility functions for time and date handling in ChronoPlan

export function parseMinutes(timeStr: string): number {
  if (!timeStr) return 0;
  const parts = timeStr.split(':');
  const hours = parseInt(parts[0], 10) || 0;
  const minutes = parseInt(parts[1], 10) || 0;
  return hours * 60 + minutes;
}

export function formatMinutesToTime(totalMinutes: number): string {
  const normalized = Math.max(0, Math.min(24 * 60 - 1, Math.floor(totalMinutes)));
  const hours = Math.floor(normalized / 60);
  const minutes = normalized % 60;
  return `${hours.toString().padStart(2, '0')}:${minutes.toString().padStart(2, '0')}`;
}

export function formatTime12h(timeStr: string, use12h: boolean = true): string {
  if (!timeStr) return '';
  if (!use12h) return timeStr;
  
  const [hStr, mStr] = timeStr.split(':');
  let hours = parseInt(hStr, 10);
  const minutes = mStr || '00';
  if (isNaN(hours)) return timeStr;
  hours = hours % 24; // "24:00" (end of day) reads as 12:00 AM
  
  const period = hours >= 12 ? 'PM' : 'AM';
  hours = hours % 12;
  if (hours === 0) hours = 12;
  return `${hours.toString().padStart(2, '0')}:${minutes} ${period}`;
}

export function formatDuration(minutes: number): string {
  if (minutes < 60) {
    return `${minutes}m`;
  }
  const hrs = Math.floor(minutes / 60);
  const mins = minutes % 60;
  return mins === 0 ? `${hrs}h` : `${hrs}h ${mins}m`;
}

export function formatDurationFromHoursMins(hours: number, minutes: number): string {
  return formatDuration(hours * 60 + minutes);
}

export function getTodayDateString(): string {
  const now = new Date();
  const year = now.getFullYear();
  const month = (now.getMonth() + 1).toString().padStart(2, '0');
  const day = now.getDate().toString().padStart(2, '0');
  return `${year}-${month}-${day}`;
}

export function formatDateReadable(dateStr: string): string {
  if (!dateStr) return '';
  const [year, month, day] = dateStr.split('-').map(Number);
  const date = new Date(year, month - 1, day);
  return date.toLocaleDateString('en-US', {
    weekday: 'short',
    month: 'short',
    day: 'numeric',
    year: 'numeric'
  });
}

export function isOverlapping(
  startA: string,
  endA: string,
  startB: string,
  endB: string
): boolean {
  const aStart = parseMinutes(startA);
  const aEnd = parseMinutes(endA);
  const bStart = parseMinutes(startB);
  const bEnd = parseMinutes(endB);
  return Math.max(aStart, bStart) < Math.min(aEnd, bEnd);
}

export function getOverlap(
  startA: string,
  endA: string,
  startB: string,
  endB: string
): { start: string; end: string; durationMinutes: number } | null {
  const aStart = parseMinutes(startA);
  const aEnd = parseMinutes(endA);
  const bStart = parseMinutes(startB);
  const bEnd = parseMinutes(endB);
  
  const maxStart = Math.max(aStart, bStart);
  const minEnd = Math.min(aEnd, bEnd);
  
  if (maxStart < minEnd) {
    return {
      start: formatMinutesToTime(maxStart),
      end: formatMinutesToTime(minEnd),
      durationMinutes: minEnd - maxStart
    };
  }
  return null;
}
