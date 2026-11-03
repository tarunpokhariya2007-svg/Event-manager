import React, { useState } from 'react';
import { PageHeader } from '../components/common/PageHeader';
import { useApp } from '../context/AppContext';
import { TaskPriority } from '../types/task';
import {
  Save,
  RotateCcw,
  Sliders,
  Clock,
  User,
  Shield,
  Terminal,
  Activity,
  CheckCircle2,
  AlertCircle
} from 'lucide-react';
import { cxxSchedulerService } from '../services/cxxSchedulerService';
import { API_ENDPOINTS } from '../config/api';

export const SettingsPage: React.FC = () => {
  const { settings, updateSettings, resetToDefaults, addToast } = useApp();

  const [userName, setUserName] = useState(settings.userName);
  const [userEmail, setUserEmail] = useState(settings.userEmail);
  const [userRole, setUserRole] = useState(settings.userRole);
  const [workHoursStart, setWorkHoursStart] = useState(settings.workHoursStart);
  const [workHoursEnd, setWorkHoursEnd] = useState(settings.workHoursEnd);
  const [defaultBreakDuration, setDefaultBreakDuration] = useState(settings.defaultBreakDuration);
  const [defaultTaskPriority, setDefaultTaskPriority] = useState<TaskPriority>(settings.defaultTaskPriority);
  const [dailyWorkloadLimit, setDailyWorkloadLimit] = useState(settings.dailyWorkloadLimit);
  const [timeFormat, setTimeFormat] = useState<'12h' | '24h'>(settings.timeFormat);
  const [allowTaskSplittingDefault, setAllowTaskSplittingDefault] = useState(settings.allowTaskSplittingDefault);
  const [autoRescheduleOnConflict, setAutoRescheduleOnConflict] = useState(settings.autoRescheduleOnConflict);
  const endpointUrl = API_ENDPOINTS.optimize;
  const [isPinging, setIsPinging] = useState(false);
  const [pingResult, setPingResult] = useState<{ online: boolean; latencyMs: number; engineVersion?: string; error?: string } | null>(null);

  const handleSave = (e: React.FormEvent) => {
    e.preventDefault();
    updateSettings({
      userName,
      userEmail,
      userRole,
      workHoursStart,
      workHoursEnd,
      defaultBreakDuration,
      defaultTaskPriority,
      dailyWorkloadLimit,
      timeFormat,
      allowTaskSplittingDefault,
      autoRescheduleOnConflict,
      cxxBackend: {
        ...settings.cxxBackend,
        endpointUrl
      }
    });
  };

  const handlePingCxx = async () => {
    setIsPinging(true);
    setPingResult(null);
    try {
      const res = await cxxSchedulerService.ping();
      setPingResult(res);
      if (res.online) {
        addToast('C++ Engine Connected', `Response in ${res.latencyMs}ms${res.engineVersion ? ` (engine ${res.engineVersion})` : ''}.`, 'success');
      } else {
        addToast('C++ Engine Unreachable', res.error ?? 'The backend did not respond.', 'error');
      }
    } finally {
      setIsPinging(false);
    }
  };

  return (
    <div className="space-y-6 max-w-4xl">
      <PageHeader
        heading="Settings"
        description="Configure your profile, working window limits, solver defaults, and backend integration parameters."
      >
        <button
          type="button"
          onClick={resetToDefaults}
          className="px-3 py-1.5 text-xs text-[#555552] hover:text-[#202020] hover:bg-[#E8E8E5] border border-[#DEDEDA] rounded-md transition-colors flex items-center gap-1.5"
        >
          <RotateCcw className="w-3.5 h-3.5" />
          <span>Reset Settings</span>
        </button>
      </PageHeader>

      <form onSubmit={handleSave} className="space-y-6 text-xs">
        {/* User Profile Section */}
        <div className="bg-white border border-[#DEDEDA] rounded-lg p-5">
          <div className="flex items-center gap-2 pb-3 border-b border-[#DEDEDA] mb-4">
            <User className="w-4 h-4 text-[#555552]" />
            <h3 className="text-sm font-semibold text-[#202020]">User Profile</h3>
          </div>

          <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
            <div>
              <label className="block font-medium text-[#202020] mb-1">Display Name</label>
              <input
                type="text"
                value={userName}
                onChange={e => setUserName(e.target.value)}
                className="w-full px-3 py-1.5 border border-[#DEDEDA] rounded-md bg-white text-xs"
              />
            </div>
            <div>
              <label className="block font-medium text-[#202020] mb-1">Email</label>
              <input
                type="email"
                value={userEmail}
                onChange={e => setUserEmail(e.target.value)}
                className="w-full px-3 py-1.5 border border-[#DEDEDA] rounded-md bg-white text-xs"
              />
            </div>
            <div>
              <label className="block font-medium text-[#202020] mb-1">Role / Department</label>
              <input
                type="text"
                value={userRole}
                onChange={e => setUserRole(e.target.value)}
                className="w-full px-3 py-1.5 border border-[#DEDEDA] rounded-md bg-white text-xs"
              />
            </div>
          </div>
        </div>

        {/* Schedule & Working Hours */}
        <div className="bg-white border border-[#DEDEDA] rounded-lg p-5">
          <div className="flex items-center gap-2 pb-3 border-b border-[#DEDEDA] mb-4">
            <Clock className="w-4 h-4 text-[#555552]" />
            <h3 className="text-sm font-semibold text-[#202020]">Working Hours &amp; Capacity</h3>
          </div>

          <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
            <div>
              <label className="block font-medium text-[#202020] mb-1">Daily Working Window</label>
              <div className="flex items-center gap-2">
                <input
                  type="time"
                  value={workHoursStart}
                  onChange={e => setWorkHoursStart(e.target.value)}
                  className="w-full px-2.5 py-1.5 border border-[#DEDEDA] rounded-md text-xs text-center"
                />
                <span className="text-[#777773]">—</span>
                <input
                  type="time"
                  value={workHoursEnd}
                  onChange={e => setWorkHoursEnd(e.target.value)}
                  className="w-full px-2.5 py-1.5 border border-[#DEDEDA] rounded-md text-xs text-center"
                />
              </div>
            </div>

            <div>
              <label className="block font-medium text-[#202020] mb-1">Default Break Duration</label>
              <select
                value={defaultBreakDuration}
                onChange={e => setDefaultBreakDuration(parseInt(e.target.value, 10))}
                className="w-full px-3 py-1.5 border border-[#DEDEDA] rounded-md bg-white text-xs"
              >
                <option value={15}>15 Minutes</option>
                <option value={30}>30 Minutes</option>
                <option value={45}>45 Minutes</option>
                <option value={60}>60 Minutes</option>
              </select>
            </div>

            <div>
              <label className="block font-medium text-[#202020] mb-1">Daily Workload Limit</label>
              <div className="flex items-center gap-2">
                <input
                  type="number"
                  step="0.5"
                  min="2"
                  max="16"
                  value={dailyWorkloadLimit}
                  onChange={e => setDailyWorkloadLimit(parseFloat(e.target.value) || 7.5)}
                  className="w-full px-3 py-1.5 border border-[#DEDEDA] rounded-md text-xs"
                />
                <span className="text-[#777773] whitespace-nowrap">hours/day</span>
              </div>
            </div>
          </div>
        </div>

        {/* Scheduling Preferences */}
        <div className="bg-white border border-[#DEDEDA] rounded-lg p-5">
          <div className="flex items-center gap-2 pb-3 border-b border-[#DEDEDA] mb-4">
            <Sliders className="w-4 h-4 text-[#555552]" />
            <h3 className="text-sm font-semibold text-[#202020]">Scheduling Preferences</h3>
          </div>

          <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
            <div>
              <label className="block font-medium text-[#202020] mb-1">Default Task Priority</label>
              <select
                value={defaultTaskPriority}
                onChange={e => setDefaultTaskPriority(e.target.value as TaskPriority)}
                className="w-full px-3 py-1.5 border border-[#DEDEDA] rounded-md bg-white text-xs"
              >
                <option value="High">High</option>
                <option value="Medium">Medium</option>
                <option value="Low">Low</option>
              </select>
            </div>

            <div>
              <label className="block font-medium text-[#202020] mb-1">Time Format</label>
              <select
                value={timeFormat}
                onChange={e => setTimeFormat(e.target.value as '12h' | '24h')}
                className="w-full px-3 py-1.5 border border-[#DEDEDA] rounded-md bg-white text-xs"
              >
                <option value="12h">12-Hour (09:00 AM / 02:00 PM)</option>
                <option value="24h">24-Hour (09:00 / 14:00)</option>
              </select>
            </div>
          </div>

          <div className="mt-4 pt-4 border-t border-[#DEDEDA] space-y-3">
            <label className="flex items-center gap-2.5 cursor-pointer">
              <input
                type="checkbox"
                checked={allowTaskSplittingDefault}
                onChange={e => setAllowTaskSplittingDefault(e.target.checked)}
                className="rounded border-[#DEDEDA] text-[#202020] focus:ring-0"
              />
              <div>
                <span className="font-medium text-[#202020]">Allow task splitting by default</span>
                <p className="text-[11px] text-[#777773]">
                  Enables large tasks to be scheduled across multiple non-contiguous time slots.
                </p>
              </div>
            </label>

            <label className="flex items-center gap-2.5 cursor-pointer">
              <input
                type="checkbox"
                checked={autoRescheduleOnConflict}
                onChange={e => setAutoRescheduleOnConflict(e.target.checked)}
                className="rounded border-[#DEDEDA] text-[#202020] focus:ring-0"
              />
              <div>
                <span className="font-medium text-[#202020]">Auto-reschedule flexible events on conflict</span>
                <p className="text-[11px] text-[#777773]">
                  Automatically accept recommended alternative slot when a flexible event overlaps with a fixed event.
                </p>
              </div>
            </label>
          </div>
        </div>

        {/* C++ Scheduling Engine Connection Settings */}
        <div className="bg-white border border-[#DEDEDA] rounded-lg p-5">
          <div className="flex items-center justify-between pb-3 border-b border-[#DEDEDA] mb-4">
            <div className="flex items-center gap-2">
              <Terminal className="w-4 h-4 text-[#555552]" />
              <h3 className="text-sm font-semibold text-[#202020]">
                C++ Scheduling Engine Integration
              </h3>
            </div>
            <div className="flex items-center gap-1.5 px-2 py-0.5 rounded bg-[#FAFAF9] border border-[#DEDEDA] text-[11px]">
              <span className="w-1.5 h-1.5 rounded-full bg-[#166534]" />
              <span className="text-[#555552]">Architecture Ready</span>
            </div>
          </div>

          <p className="text-xs text-[#777773] mb-4 leading-relaxed">
            Per system architecture, all optimization algorithms are executed by the native C++ engine. The frontend sends scheduling requests to the C++ backend over HTTP; the backend URL is set with the VITE_API_BASE_URL environment variable.
          </p>

          <div className="grid grid-cols-1 md:grid-cols-3 gap-4 mb-4">
            <div className="md:col-span-2">
              <label className="block font-medium text-[#202020] mb-1">C++ Solver REST / IPC Endpoint</label>
              <input
                type="text"
                value={API_ENDPOINTS.optimize}
                readOnly
                placeholder="http://localhost:8080/api/v1/schedule/optimize"
                className="w-full px-3 py-1.5 border border-[#DEDEDA] rounded-md font-mono text-xs"
              />
            </div>
            <div className="flex items-end">
              <button
                type="button"
                onClick={handlePingCxx}
                disabled={isPinging}
                className="w-full py-1.5 px-3 bg-[#EFEFED] hover:bg-[#E8E8E5] text-[#202020] border border-[#DEDEDA] rounded-md font-medium transition-colors flex items-center justify-center gap-1.5"
              >
                <Activity className="w-3.5 h-3.5" />
                <span>{isPinging ? 'Pinging...' : 'Test Connection'}</span>
              </button>
            </div>
          </div>

          {pingResult && (
            <div className={`p-3 rounded-md text-xs border ${
              pingResult.online ? 'bg-[#F0FDF4] border-[#BBF7D0] text-[#166534]' : 'bg-[#FAFAF9] border-[#DEDEDA] text-[#555552]'
            }`}>
              {pingResult.online ? (
                <div className="flex items-center gap-2">
                  <CheckCircle2 className="w-4 h-4" />
                  <span>Native C++ Solver Connected: {pingResult.latencyMs}ms roundtrip latency.</span>
                </div>
              ) : (
                <div className="flex items-center gap-2">
                  <span className="w-2 h-2 rounded-full bg-[#888884]" />
                  <span>The C++ backend is not responding{pingResult.error ? `: ${pingResult.error}` : '.'} Scheduling is unavailable until it is reachable.</span>
                </div>
              )}
            </div>
          )}
        </div>

        {/* Submit */}
        <div className="flex items-center justify-end gap-3 pt-2">
          <button
            type="submit"
            className="inline-flex items-center gap-2 px-5 py-2 bg-[#202020] hover:bg-[#333330] text-white font-medium rounded-md shadow-xs transition-colors"
          >
            <Save className="w-3.5 h-3.5" />
            <span>Save Settings</span>
          </button>
        </div>
      </form>
    </div>
  );
};
