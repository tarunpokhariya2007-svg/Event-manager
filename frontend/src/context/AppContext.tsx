import React, { createContext, useContext, useState, useEffect, useMemo } from 'react';
import { Task } from '../types/task';
import { CalendarEvent } from '../types/event';
import { UserSettings } from '../types/settings';
import { PlannerConfig, SchedulingResult, ScheduleHistoryEntry } from '../types/planner';
import { SchedulingConflict, AlternativeTimeSuggestion } from '../types/conflict';
import { storageService } from '../services/storageService';
import { scheduleHistoryService } from '../services/scheduleHistoryService';
import { cxxSchedulerService } from '../services/cxxSchedulerService';
import { backendDataService } from '../services/backendDataService';
import { errorMessage } from '../services/apiClient';
import { alternativesFromSlots, freeSlotsForDay, pickMovableEvent } from '../utils/conflictUtils';
import { getTodayDateString, isOverlapping, getOverlap, parseMinutes } from '../utils/timeUtils';

export interface ToastMessage {
  id: string;
  title: string;
  description?: string;
  type: 'info' | 'success' | 'warning' | 'error';
}

interface AppContextType {
  tasks: Task[];
  events: CalendarEvent[];
  settings: UserSettings;
  conflicts: SchedulingConflict[];
  resolvedConflictIds: string[];
  plannerConfig: PlannerConfig;
  schedulingResult: SchedulingResult | null;
  isGeneratingSchedule: boolean;
  scheduleError: string | null;
  scheduleHistory: ScheduleHistoryEntry[];
  deleteHistoryEntry: (id: string) => void;
  clearScheduleHistory: () => void;
  backendStatus: 'loading' | 'online' | 'offline';
  backendError: string | null;
  reloadData: () => Promise<void>;
  toasts: ToastMessage[];
  searchQuery: string;
  
  // Modals & Selection state
  activeModal: 'add_task' | 'edit_task' | 'add_event' | 'edit_event' | 'event_details' | 'command_palette' | null;
  selectedTask: Task | null;
  selectedEvent: CalendarEvent | null;

  // Actions
  addTask: (task: Omit<Task, 'id'>) => void;
  updateTask: (task: Task) => void;
  deleteTask: (id: string) => void;
  toggleTaskComplete: (id: string) => void;

  addEvent: (event: Omit<CalendarEvent, 'id'>) => void;
  updateEvent: (event: CalendarEvent) => void;
  deleteEvent: (id: string) => void;
  resolveConflictWithAlternative: (conflictId: string, eventIdToUpdate: string, alt: AlternativeTimeSuggestion) => Promise<void>;

  updatePlannerConfig: (partial: Partial<PlannerConfig>) => void;
  generateSchedule: () => Promise<void>;
  applyScheduleToCalendar: () => void;

  updateSettings: (newSettings: Partial<UserSettings>) => void;
  resetToDefaults: () => void;

  addToast: (title: string, description?: string, type?: ToastMessage['type']) => void;
  removeToast: (id: string) => void;

  setSearchQuery: (query: string) => void;
  setActiveModal: (modal: 'add_task' | 'edit_task' | 'add_event' | 'edit_event' | 'event_details' | 'command_palette' | null) => void;
  setSelectedTask: (task: Task | null) => void;
  setSelectedEvent: (event: CalendarEvent | null) => void;
  openCreateEventWithSlot: (date: string, startTime: string, endTime: string) => void;
}

const AppContext = createContext<AppContextType | undefined>(undefined);

export const AppProvider: React.FC<{ children: React.ReactNode }> = ({ children }) => {
  const [tasks, setTasks] = useState<Task[]>([]);
  const [events, setEvents] = useState<CalendarEvent[]>([]);
  const [settings, setSettings] = useState<UserSettings>(() => storageService.loadSettings());
  const [toasts, setToasts] = useState<ToastMessage[]>([]);
  const [searchQuery, setSearchQuery] = useState('');

  const [activeModal, setActiveModal] = useState<AppContextType['activeModal']>(null);
  const [selectedTask, setSelectedTask] = useState<Task | null>(null);
  const [selectedEvent, setSelectedEvent] = useState<CalendarEvent | null>(null);

  const [resolvedConflictIds, setResolvedConflictIds] = useState<string[]>([]);
  const [isGeneratingSchedule, setIsGeneratingSchedule] = useState(false);
  const [schedulingResult, setSchedulingResult] = useState<SchedulingResult | null>(null);
  const [scheduleError, setScheduleError] = useState<string | null>(null);
  const [scheduleHistory, setScheduleHistory] = useState<ScheduleHistoryEntry[]>(() => scheduleHistoryService.load());
  const [backendStatus, setBackendStatus] = useState<'loading' | 'online' | 'offline'>('loading');
  const [backendError, setBackendError] = useState<string | null>(null);

  const today = getTodayDateString();

  const [plannerConfig, setPlannerConfig] = useState<PlannerConfig>({
    date: today,
    workHoursStart: '08:30',
    workHoursEnd: '18:30',
    minBreakDuration: 30,
    includePendingTasks: true,
    allowTaskSplitting: true,
    maxDailyWorkload: 7.5,
    optimizationGoal: 'balanced'
  });

  // Tasks and events live in the C++ backend (SQLite); only settings are persisted locally
  useEffect(() => {
    storageService.saveSettings(settings);
  }, [settings]);

  // Toast Helper
  const addToast = (title: string, description?: string, type: ToastMessage['type'] = 'info') => {
    const id = `toast-${Date.now()}-${Math.random().toString(36).substring(2, 5)}`;
    setToasts(prev => [...prev, { id, title, description, type }]);
    setTimeout(() => {
      removeToast(id);
    }, 4500);
  };

  const removeToast = (id: string) => {
    setToasts(prev => prev.filter(t => t.id !== id));
  };

  // Load tasks and events from the C++ backend
  const reloadData = async () => {
    setBackendStatus('loading');
    try {
      const [loadedTasks, loadedEvents] = await Promise.all([backendDataService.listTasks(), backendDataService.listEvents()]);
      setTasks(loadedTasks);
      setEvents(loadedEvents);
      setBackendStatus('online');
      setBackendError(null);
    } catch (e) {
      setBackendStatus('offline');
      setBackendError(errorMessage(e));
      addToast('Backend unavailable', errorMessage(e), 'error');
    }
  };

  useEffect(() => {
    reloadData();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  const fail = (title: string, e: unknown) => addToast(title, errorMessage(e), 'error');

  // Detect conflicts per day (events are grouped by date first, so only same-day pairs are compared)
  const conflicts = useMemo(() => {
    const byDate = new Map<string, CalendarEvent[]>();
    for (const e of events) {
      const day = byDate.get(e.date);
      if (day) day.push(e);
      else byDate.set(e.date, [e]);
    }

    const list: SchedulingConflict[] = [];
    for (const [date, dayEvents] of byDate) {
      if (dayEvents.length < 2) continue;
      for (let i = 0; i < dayEvents.length; i++) {
        for (let j = i + 1; j < dayEvents.length; j++) {
          const a = dayEvents[i];
          const b = dayEvents[j];
          if (!isOverlapping(a.startTime, a.endTime, b.startTime, b.endTime)) continue;
          const conflictId = `conflict-${a.id}-${b.id}`;
          if (resolvedConflictIds.includes(conflictId)) continue;

          const overlap = getOverlap(a.startTime, a.endTime, b.startTime, b.endTime);
          // Alternatives are sized for, and computed without, the event that would be moved.
          const movable = pickMovableEvent(a, b);
          const others = dayEvents.filter(e => e.id !== movable.id);
          list.push({
            id: conflictId,
            eventA: a,
            eventB: b,
            date,
            overlapStart: overlap ? overlap.start : a.startTime,
            overlapEnd: overlap ? overlap.end : a.endTime,
            description: `The selected time for "${b.title}" conflicts with "${a.title}".`,
            status: 'unresolved',
            suggestedAlternatives: alternativesFromSlots(
              freeSlotsForDay(date, others, settings.workHoursStart, settings.workHoursEnd),
              parseMinutes(movable.endTime) - parseMinutes(movable.startTime),
              others
            )
          });
        }
      }
    }
    return list.sort((x, y) => x.date.localeCompare(y.date) || x.overlapStart.localeCompare(y.overlapStart));
  }, [events, resolvedConflictIds, settings.workHoursStart, settings.workHoursEnd]);

  // Task Actions (backend: POST/PUT/DELETE /api/v1/tasks)
  const addTask = async (newTaskData: Omit<Task, 'id'>) => {
    try {
      const created = await backendDataService.createTask(newTaskData);
      setTasks(prev => [created, ...prev]);
      addToast('Task created', `"${created.title}" was added.`, 'success');
    } catch (e) {
      fail('Could not create task', e);
    }
  };

  const updateTask = async (updatedTask: Task) => {
    try {
      const saved = await backendDataService.updateTask(updatedTask);
      setTasks(prev => prev.map(t => (t.id === saved.id ? saved : t)));
      addToast('Task updated', `"${saved.title}" changes saved.`, 'info');
    } catch (e) {
      fail('Could not update task', e);
    }
  };

  const deleteTask = async (id: string) => {
    const target = tasks.find(t => t.id === id);
    try {
      await backendDataService.deleteTask(id);
      // The backend also drops this task from other tasks' prerequisites, so refresh the list.
      setTasks(await backendDataService.listTasks());
      addToast('Task removed', target ? `"${target.title}" was deleted.` : 'Task deleted.', 'info');
    } catch (e) {
      fail('Could not delete task', e);
    }
  };

  const toggleTaskComplete = async (id: string) => {
    const target = tasks.find(t => t.id === id);
    if (!target) return;
    const isDone = target.status !== 'Completed';
    const next: Task = {
      ...target,
      status: isDone ? 'Completed' : 'Pending',
      completedAt: isDone ? new Date().toISOString() : undefined
    };
    try {
      const saved = await backendDataService.updateTask(next);
      setTasks(prev => prev.map(t => (t.id === saved.id ? saved : t)));
      addToast(isDone ? 'Task completed' : 'Task marked pending', target.title, isDone ? 'success' : 'info');
    } catch (e) {
      fail('Could not update task', e);
    }
  };

  // Event Actions (backend: POST/PUT/DELETE /api/v1/events)
  const addEvent = async (newEventData: Omit<CalendarEvent, 'id'>) => {
    try {
      const created = await backendDataService.createEvent(newEventData);
      setEvents(prev => [...prev, created]);
      addToast('Event scheduled', `"${created.title}" on ${created.date}.`, 'success');
    } catch (e) {
      fail('Could not create event', e);
    }
  };

  const updateEvent = async (updatedEvent: CalendarEvent) => {
    try {
      const saved = await backendDataService.updateEvent(updatedEvent);
      setEvents(prev => prev.map(e => (e.id === saved.id ? saved : e)));
      addToast('Event updated', `"${saved.title}" was updated.`, 'info');
    } catch (e) {
      fail('Could not update event', e);
    }
  };

  const deleteEvent = async (id: string) => {
    const target = events.find(e => e.id === id);
    try {
      await backendDataService.deleteEvent(id);
      setEvents(prev => prev.filter(e => e.id !== id));
      addToast('Event deleted', target ? `"${target.title}" removed.` : 'Event deleted.', 'info');
    } catch (e) {
      fail('Could not delete event', e);
    }
  };

  const resolveConflictWithAlternative = async (
    conflictId: string,
    eventIdToUpdate: string,
    alt: AlternativeTimeSuggestion
  ) => {
    const target = events.find(e => e.id === eventIdToUpdate);
    if (!target) return;
    try {
      const saved = await backendDataService.updateEvent({
        ...target,
        startTime: alt.startTime,
        endTime: alt.endTime,
        date: alt.date || target.date
      });
      setEvents(prev => prev.map(e => (e.id === saved.id ? saved : e)));
      setResolvedConflictIds(prev => [...prev, conflictId]);
      addToast('Conflict resolved', `Rescheduled to ${alt.label}.`, 'success');
    } catch (e) {
      fail('Could not resolve conflict', e);
    }
  };

  // Planner
  const updatePlannerConfig = (partial: Partial<PlannerConfig>) => {
    setPlannerConfig(prev => ({ ...prev, ...partial }));
  };

  const generateSchedule = async () => {
    setIsGeneratingSchedule(true);
    setScheduleError(null);
    try {
      const res = await cxxSchedulerService.generateSchedule(plannerConfig, events, tasks);
      setSchedulingResult(res);
      setScheduleHistory(scheduleHistoryService.add(res));
      if (res.status === 'success') {
        addToast('Schedule optimized', `C++ engine placed ${res.allocatedTasksCount} task(s) in ${res.solverDurationMs.toFixed(1)} ms.`, 'success');
      } else if (res.status === 'partial') {
        addToast('Partial schedule', `${res.unscheduledTasks.length} task(s) could not be fully placed.`, 'warning');
      } else {
        addToast('No feasible schedule', 'The engine could not place the requested tasks.', 'warning');
      }
    } catch (e) {
      // No simulated fallback: the failure is shown, and any stale result is cleared.
      setSchedulingResult(null);
      setScheduleError(errorMessage(e));
      addToast('Optimization failed', errorMessage(e), 'error');
    } finally {
      setIsGeneratingSchedule(false);
    }
  };

  const applyScheduleToCalendar = () => {
    if (!schedulingResult) return;
    addToast('Preview only', 'The backend has no apply endpoint yet, so this schedule is a read-only preview and no events were changed.', 'info');
  };

  const updateSettings = (newSettings: Partial<UserSettings>) => {
    setSettings(prev => ({ ...prev, ...newSettings }));
    addToast('Settings saved', 'Configuration updated.', 'info');
  };

  const resetToDefaults = () => {
    setSettings(storageService.resetSettings());
    setResolvedConflictIds([]);
    setSchedulingResult(null);
    setScheduleError(null);
    setScheduleHistory(scheduleHistoryService.clear());
    addToast('Settings reset', 'Local settings restored. Tasks and events stored in the backend were not changed.', 'info');
  };

  const openCreateEventWithSlot = (date: string, startTime: string, endTime: string) => {
    setSelectedEvent({
      id: '',
      title: '',
      date,
      startTime,
      endTime,
      category: 'Meeting',
      priority: 'Medium',
      flexibility: 'Fixed'
    });
    setActiveModal('add_event');
  };

  return (
    <AppContext.Provider
      value={{
        tasks,
        events,
        settings,
        conflicts,
        resolvedConflictIds,
        plannerConfig,
        schedulingResult,
        isGeneratingSchedule,
        scheduleError,
        scheduleHistory,
        deleteHistoryEntry: (id: string) => setScheduleHistory(scheduleHistoryService.remove(id)),
        clearScheduleHistory: () => setScheduleHistory(scheduleHistoryService.clear()),
        backendStatus,
        backendError,
        reloadData,
        toasts,
        searchQuery,
        activeModal,
        selectedTask,
        selectedEvent,
        addTask,
        updateTask,
        deleteTask,
        toggleTaskComplete,
        addEvent,
        updateEvent,
        deleteEvent,
        resolveConflictWithAlternative,
        updatePlannerConfig,
        generateSchedule,
        applyScheduleToCalendar,
        updateSettings,
        resetToDefaults,
        addToast,
        removeToast,
        setSearchQuery,
        setActiveModal,
        setSelectedTask,
        setSelectedEvent,
        openCreateEventWithSlot
      }}
    >
      {children}
    </AppContext.Provider>
  );
};

export const useApp = () => {
  const context = useContext(AppContext);
  if (!context) {
    throw new Error('useApp must be used within an AppProvider');
  }
  return context;
};
