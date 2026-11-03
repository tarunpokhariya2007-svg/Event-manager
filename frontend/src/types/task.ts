export type TaskPriority = 'High' | 'Medium' | 'Low';

export type TaskStatus = 'Pending' | 'In Progress' | 'Completed' | 'Overdue' | 'Blocked';

export interface Task {
  id: string;
  title: string;
  description?: string;
  estimatedDuration: number; // in minutes
  priority: TaskPriority;
  deadline: string; // ISO string or YYYY-MM-DDTHH:mm
  earliestStartDate?: string;
  latestAcceptableCompletionDate?: string;
  allowSplitting: boolean;
  dependencies: string[]; // array of Task IDs
  status: TaskStatus;
  completedAt?: string;
  category?: string;
  allocatedSlot?: {
    date: string;
    startTime: string;
    endTime: string;
  };
}

export type TaskFilterPriority = 'All' | TaskPriority;
export type TaskFilterStatus = 'All' | TaskStatus;
export type TaskSortField = 'deadline' | 'priority' | 'estimatedDuration' | 'title';
export type SortOrder = 'asc' | 'desc';
