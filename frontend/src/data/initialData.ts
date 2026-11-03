import { Task } from '../types/task';
import { CalendarEvent } from '../types/event';
import { UserSettings } from '../types/settings';
import { getTodayDateString } from '../utils/timeUtils';
import { API_ENDPOINTS } from '../config/api';

const today = getTodayDateString();

export const initialUserSettings: UserSettings = {
  userName: 'Tarun',
  userEmail: 'tarun.sharma@chronoplan.dev',
  userRole: 'Senior CS & Systems Engineering',
  workHoursStart: '08:30',
  workHoursEnd: '18:30',
  defaultBreakDuration: 30,
  defaultTaskPriority: 'Medium',
  dailyWorkloadLimit: 7.5,
  timeFormat: '12h',
  allowTaskSplittingDefault: true,
  autoRescheduleOnConflict: false,
  minBufferBetweenEvents: 10,
  cxxBackend: {
    enabled: true,
    endpointUrl: API_ENDPOINTS.optimize,
    timeoutMs: 3000,
    logDebugDetails: true,
    lastPingStatus: 'standby',
    latencyMs: 12
  }
};

export const initialEvents: CalendarEvent[] = [
  {
    id: 'evt-1',
    title: 'DSA Lecture (Graphs & Topological Sort)',
    description: 'Prof. Ramanujan - Hall B302. Mandatory lecture on DAGs and path optimization.',
    date: today,
    startTime: '09:00',
    endTime: '10:00',
    location: 'Auditorium Hall B302',
    category: 'Lecture',
    priority: 'High',
    flexibility: 'Fixed',
    isRecurring: true
  },
  {
    id: 'evt-2',
    title: 'Capstone Project Meeting',
    description: 'Sprint checkpoint with Dr. Mehta regarding C++ backend interface and benchmark metrics.',
    date: today,
    startTime: '14:00',
    endTime: '15:00',
    location: 'Lab Room 412 / Zoom',
    category: 'Meeting',
    priority: 'High',
    flexibility: 'Fixed'
  },
  {
    id: 'evt-3',
    title: 'Department Research Colloquium',
    description: 'Overlapping seminar on High Performance Scheduling in Distributed Clusters.',
    date: today,
    startTime: '14:30', // Intentionally overlaps with Capstone Meeting (14:00 - 15:00) to showcase Conflict Resolution
    endTime: '15:30',
    location: 'Conference Hall A',
    category: 'Academic',
    priority: 'Medium',
    flexibility: 'Flexible'
  },
  {
    id: 'evt-4',
    title: 'Distributed Systems Office Hours',
    description: 'Q&A on Paxos consensus implementation and state machine replication.',
    date: today,
    startTime: '17:30',
    endTime: '18:15',
    location: 'Faculty Cabin 204',
    category: 'Academic',
    priority: 'Medium',
    flexibility: 'Flexible'
  }
];

export const initialTasks: Task[] = [
  {
    id: 'task-1',
    title: 'DSA Assignment: Red-Black Trees & DAG Ordering',
    description: 'Implement balance invariant checks and topological sort cycle detectors in C++.',
    estimatedDuration: 120, // 2 hours
    priority: 'High',
    deadline: `${today}T17:00`,
    earliestStartDate: `${today}T08:00`,
    latestAcceptableCompletionDate: `${today}T17:00`,
    allowSplitting: false,
    dependencies: [],
    status: 'In Progress',
    category: 'Academic'
  },
  {
    id: 'task-2',
    title: 'DBMS Revision: B+ Tree Indexing & WAL',
    description: 'Review Chapter 8 on write-ahead logging, recovery protocols (ARIES), and buffer pools.',
    estimatedDuration: 60, // 1 hour
    priority: 'Medium',
    deadline: `${today}T20:00`,
    earliestStartDate: `${today}T10:00`,
    latestAcceptableCompletionDate: `${today}T22:00`,
    allowSplitting: true,
    dependencies: [],
    status: 'Pending',
    category: 'Academic'
  },
  {
    id: 'task-3',
    title: 'Project Work: C++ Scheduler Bridge Integration',
    description: 'Wire IPC socket wrapper between web server and native C++ optimization engine.',
    estimatedDuration: 120, // 2 hours
    priority: 'High',
    deadline: `${today}T18:30`,
    earliestStartDate: `${today}T11:00`,
    latestAcceptableCompletionDate: `${today}T21:00`,
    allowSplitting: true,
    dependencies: [],
    status: 'Pending',
    category: 'Work'
  },
  {
    id: 'task-4',
    title: 'Literature Research: Heuristic Scheduling Algorithms',
    description: 'Survey greedy interval scheduling and constraint satisfaction solver literature.',
    estimatedDuration: 90,
    priority: 'Medium',
    deadline: `${today}T23:59`,
    allowSplitting: false,
    dependencies: [],
    status: 'Completed',
    category: 'Research',
    completedAt: `${today}T08:45`
  },
  {
    id: 'task-5',
    title: 'Prepare Presentation Slides',
    description: 'Synthesize research literature into 12 concise technical slides with architecture diagrams.',
    estimatedDuration: 90,
    priority: 'High',
    deadline: `${today}T21:00`,
    allowSplitting: true,
    dependencies: ['task-4'], // Depends on Literature Research (Completed -> Ready!)
    status: 'Pending',
    category: 'Research'
  },
  {
    id: 'task-6',
    title: 'Review Presentation Slides with Advisor',
    description: 'Collect advisor feedback on benchmark methodology and constraint equations.',
    estimatedDuration: 45,
    priority: 'Medium',
    deadline: `${today}T22:00`,
    allowSplitting: false,
    dependencies: ['task-5'], // Depends on Prepare Slides (Pending -> Blocked!)
    status: 'Blocked',
    category: 'Research'
  },
  {
    id: 'task-7',
    title: 'Submit Final Presentation & Report',
    description: 'Submit camera-ready presentation deck to the academic portal.',
    estimatedDuration: 30,
    priority: 'High',
    deadline: `${today}T23:59`,
    allowSplitting: false,
    dependencies: ['task-6'], // Depends on Review Slides (Blocked -> Blocked!)
    status: 'Blocked',
    category: 'Research'
  },
  {
    id: 'task-8',
    title: 'OS Kernel Module Implementation',
    description: 'Kernel space virtual memory mapping allocator. Long task exceeding single-day slot window.',
    estimatedDuration: 210, // 3.5 hours
    priority: 'Low',
    deadline: `${today}T19:00`,
    allowSplitting: false,
    dependencies: [],
    status: 'Pending',
    category: 'Academic'
  }
];
