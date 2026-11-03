import React from 'react';
import { useApp } from '../../context/AppContext';
import { Sparkles, Loader2, Calendar, Clock, Sliders, ShieldAlert } from 'lucide-react';
import { getTodayDateString } from '../../utils/timeUtils';

export const PlannerConfigPanel: React.FC = () => {
  const {
    plannerConfig,
    updatePlannerConfig,
    generateSchedule,
    isGeneratingSchedule,
    settings
  } = useApp();

  const today = getTodayDateString();

  return (
    <div className="bg-white border border-[#DEDEDA] rounded-lg shadow-xs p-5">
      <div className="flex items-center justify-between pb-3 border-b border-[#DEDEDA] mb-4">
        <div>
          <h2 className="text-sm font-semibold text-[#202020]">
            Optimization Parameters
          </h2>
          <p className="text-xs text-[#777773] mt-0.5">
            Configure solver constraints for the C++ scheduling engine.
          </p>
        </div>
        <div className="flex items-center gap-1.5 px-2 py-0.5 bg-[#F7F7F5] border border-[#DEDEDA] rounded text-[11px] text-[#555552]">
          <span className="w-1.5 h-1.5 rounded-full bg-[#166534]" />
          <span>C++ Engine: Standby / Ready</span>
        </div>
      </div>

      <div className="grid grid-cols-1 md:grid-cols-3 lg:grid-cols-4 gap-4 text-xs">
        {/* Date Selection */}
        <div>
          <label className="block font-medium text-[#202020] mb-1">
            Target Planning Date
          </label>
          <input
            type="date"
            value={plannerConfig.date}
            onChange={e => updatePlannerConfig({ date: e.target.value })}
            className="w-full px-3 py-1.5 border border-[#DEDEDA] rounded-md text-xs bg-white text-[#202020]"
          />
        </div>

        {/* Working Hours Start & End */}
        <div>
          <label className="block font-medium text-[#202020] mb-1">
            Working Window (Start – End)
          </label>
          <div className="flex items-center gap-1.5">
            <input
              type="time"
              value={plannerConfig.workHoursStart}
              onChange={e => updatePlannerConfig({ workHoursStart: e.target.value })}
              className="w-full px-2.5 py-1.5 border border-[#DEDEDA] rounded-md text-xs text-center"
            />
            <span className="text-[#777773]">—</span>
            <input
              type="time"
              value={plannerConfig.workHoursEnd}
              onChange={e => updatePlannerConfig({ workHoursEnd: e.target.value })}
              className="w-full px-2.5 py-1.5 border border-[#DEDEDA] rounded-md text-xs text-center"
            />
          </div>
        </div>

        {/* Minimum Break Duration */}
        <div>
          <label className="block font-medium text-[#202020] mb-1">
            Minimum Break Duration
          </label>
          <select
            value={plannerConfig.minBreakDuration}
            onChange={e => updatePlannerConfig({ minBreakDuration: parseInt(e.target.value, 10) })}
            className="w-full px-3 py-1.5 border border-[#DEDEDA] rounded-md text-xs bg-white text-[#202020]"
          >
            <option value={15}>15 Minutes (Short pause)</option>
            <option value={30}>30 Minutes (Recommended)</option>
            <option value={45}>45 Minutes (Extended)</option>
            <option value={60}>60 Minutes (Full hour break)</option>
          </select>
        </div>

        {/* Max Daily Workload */}
        <div>
          <label className="block font-medium text-[#202020] mb-1">
            Max Daily Workload
          </label>
          <div className="flex items-center gap-2">
            <input
              type="number"
              step="0.5"
              min="2"
              max="16"
              value={plannerConfig.maxDailyWorkload}
              onChange={e => updatePlannerConfig({ maxDailyWorkload: parseFloat(e.target.value) || 7.5 })}
              className="w-full px-3 py-1.5 border border-[#DEDEDA] rounded-md text-xs"
            />
            <span className="text-[#777773] whitespace-nowrap">hours</span>
          </div>
        </div>
      </div>

      {/* Toggles & Options */}
      <div className="mt-4 pt-4 border-t border-[#DEDEDA] flex flex-col sm:flex-row sm:items-center justify-between gap-4 text-xs">
        <div className="flex items-center gap-6">
          <label className="flex items-center gap-2 cursor-pointer select-none">
            <input
              type="checkbox"
              checked={plannerConfig.includePendingTasks}
              onChange={e => updatePlannerConfig({ includePendingTasks: e.target.checked })}
              className="rounded border-[#DEDEDA] text-[#202020] focus:ring-0"
            />
            <span className="text-[#202020] font-medium">Include all pending tasks</span>
          </label>

          <label className="flex items-center gap-2 cursor-pointer select-none">
            <input
              type="checkbox"
              checked={plannerConfig.allowTaskSplitting}
              onChange={e => updatePlannerConfig({ allowTaskSplitting: e.target.checked })}
              className="rounded border-[#DEDEDA] text-[#202020] focus:ring-0"
            />
            <span className="text-[#202020] font-medium">Allow task splitting</span>
          </label>
        </div>

        {/* Main Prominent Button: Generate Schedule */}
        <button
          onClick={generateSchedule}
          disabled={isGeneratingSchedule}
          className="inline-flex items-center justify-center gap-2 px-5 py-2 bg-[#202020] hover:bg-[#333330] disabled:bg-[#777773] text-white text-xs font-semibold rounded-md shadow-xs transition-colors flex-shrink-0"
        >
          {isGeneratingSchedule ? (
            <>
              <Loader2 className="w-3.5 h-3.5 animate-spin" />
              <span>Optimizing Schedule (C++ Solvers)...</span>
            </>
          ) : (
            <>
              <Sparkles className="w-3.5 h-3.5" />
              <span>Generate Schedule</span>
            </>
          )}
        </button>
      </div>
    </div>
  );
};
