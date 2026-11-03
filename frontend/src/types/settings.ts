import { TaskPriority } from './task';

export interface UserSettings {
  userName: string;
  userEmail: string;
  userRole: string;
  workHoursStart: string; // "09:00"
  workHoursEnd: string;   // "18:00"
  defaultBreakDuration: number; // in minutes (e.g. 30)
  defaultTaskPriority: TaskPriority;
  dailyWorkloadLimit: number; // in hours (e.g. 7.5)
  timeFormat: '12h' | '24h';
  allowTaskSplittingDefault: boolean;
  autoRescheduleOnConflict: boolean;
  minBufferBetweenEvents: number; // in minutes
  // C++ engine backend integration configuration
  cxxBackend: {
    enabled: boolean;
    endpointUrl: string;
    timeoutMs: number;
    logDebugDetails: boolean;
    lastPingStatus: 'online' | 'standby' | 'error';
    latencyMs: number;
  };
}
