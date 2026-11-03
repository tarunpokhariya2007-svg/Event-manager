export type EventCategory = 'Academic' | 'Work' | 'Meeting' | 'Lecture' | 'Personal' | 'Milestone';

export type EventFlexibility = 'Fixed' | 'Flexible';

export type EventPriority = 'High' | 'Medium' | 'Low';

export interface CalendarEvent {
  id: string;
  title: string;
  description?: string;
  date: string; // YYYY-MM-DD
  startTime: string; // HH:mm (e.g. "09:00")
  endTime: string; // HH:mm (e.g. "10:00")
  location?: string;
  category: EventCategory;
  priority: EventPriority;
  flexibility: EventFlexibility;
  isRecurring?: boolean;
}

export type EventFilterCategory = 'All' | EventCategory;
export type EventSortField = 'date' | 'startTime' | 'priority' | 'title';
