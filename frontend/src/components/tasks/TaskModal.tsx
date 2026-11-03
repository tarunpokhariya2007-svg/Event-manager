import React, { useState, useEffect } from 'react';
import { useApp } from '../../context/AppContext';
import { Task, TaskPriority } from '../../types/task';
import { X, Clock, AlertCircle } from 'lucide-react';
import { getTodayDateString } from '../../utils/timeUtils';

export const TaskModal: React.FC = () => {
  const { activeModal, setActiveModal, selectedTask, setSelectedTask, addTask, updateTask, tasks } = useApp();

  const isEditing = activeModal === 'edit_task';
  const isOpen = activeModal === 'add_task' || activeModal === 'edit_task';

  const today = getTodayDateString();

  const [title, setTitle] = useState('');
  const [description, setDescription] = useState('');
  const [durationHours, setDurationHours] = useState(1);
  const [durationMinutes, setDurationMinutes] = useState(0);
  const [priority, setPriority] = useState<TaskPriority>('Medium');
  const [deadline, setDeadline] = useState(`${today}T18:00`);
  const [earliestStartDate, setEarliestStartDate] = useState(`${today}T09:00`);
  const [latestAcceptableCompletionDate, setLatestAcceptableCompletionDate] = useState(`${today}T21:00`);
  const [allowSplitting, setAllowSplitting] = useState(true);
  const [dependencies, setDependencies] = useState<string[]>([]);
  const [category, setCategory] = useState('Academic');
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    if (isEditing && selectedTask) {
      setTitle(selectedTask.title);
      setDescription(selectedTask.description || '');
      const totalMins = selectedTask.estimatedDuration || 60;
      setDurationHours(Math.floor(totalMins / 60));
      setDurationMinutes(totalMins % 60);
      setPriority(selectedTask.priority);
      setDeadline(selectedTask.deadline || `${today}T18:00`);
      setEarliestStartDate(selectedTask.earliestStartDate || `${today}T09:00`);
      setLatestAcceptableCompletionDate(selectedTask.latestAcceptableCompletionDate || `${today}T21:00`);
      setAllowSplitting(selectedTask.allowSplitting ?? true);
      setDependencies(selectedTask.dependencies || []);
      setCategory(selectedTask.category || 'Academic');
      setError(null);
    } else if (activeModal === 'add_task') {
      setTitle('');
      setDescription('');
      setDurationHours(1);
      setDurationMinutes(30);
      setPriority('Medium');
      setDeadline(`${today}T18:00`);
      setEarliestStartDate(`${today}T09:00`);
      setLatestAcceptableCompletionDate(`${today}T21:00`);
      setAllowSplitting(true);
      setDependencies([]);
      setCategory('Academic');
      setError(null);
    }
  }, [activeModal, selectedTask, isEditing, today]);

  if (!isOpen) return null;

  const handleClose = () => {
    setActiveModal(null);
    setSelectedTask(null);
  };

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault();
    if (!title.trim()) {
      setError('Task title is required.');
      return;
    }

    const totalDuration = durationHours * 60 + durationMinutes;
    if (totalDuration <= 0) {
      setError('Estimated duration must be greater than 0 minutes.');
      return;
    }

    if (isEditing && selectedTask) {
      updateTask({
        ...selectedTask,
        title: title.trim(),
        description: description.trim(),
        estimatedDuration: totalDuration,
        priority,
        deadline,
        earliestStartDate,
        latestAcceptableCompletionDate,
        allowSplitting,
        dependencies,
        category
      });
    } else {
      addTask({
        title: title.trim(),
        description: description.trim(),
        estimatedDuration: totalDuration,
        priority,
        deadline,
        earliestStartDate,
        latestAcceptableCompletionDate,
        allowSplitting,
        dependencies,
        category,
        status: 'Pending'
      });
    }

    handleClose();
  };

  const toggleDependency = (depId: string) => {
    setDependencies(prev =>
      prev.includes(depId) ? prev.filter(id => id !== depId) : [...prev, depId]
    );
  };

  // Exclude current task from its own dependencies
  const availableDependencies = tasks.filter(t => !selectedTask || t.id !== selectedTask.id);

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/40 backdrop-blur-[1px] p-4">
      <div
        className="w-full max-w-xl bg-white border border-[#DEDEDA] rounded-lg shadow-xl overflow-hidden flex flex-col max-h-[90vh]"
        onClick={e => e.stopPropagation()}
      >
        {/* Header */}
        <div className="flex items-center justify-between px-5 py-3.5 border-b border-[#DEDEDA] bg-[#FAFAF9]">
          <h3 className="text-sm font-semibold text-[#202020]">
            {isEditing ? 'Edit Task' : 'Add New Task'}
          </h3>
          <button
            onClick={handleClose}
            className="text-[#777773] hover:text-[#202020] p-1 rounded hover:bg-[#E8E8E5] transition-colors"
          >
            <X className="w-4 h-4" />
          </button>
        </div>

        {/* Form Body */}
        <form onSubmit={handleSubmit} className="overflow-y-auto p-5 space-y-4 text-xs">
          {error && (
            <div className="flex items-center gap-2 p-2.5 rounded bg-[#FEF2F2] border border-[#FECACA] text-[#991B1B]">
              <AlertCircle className="w-4 h-4 flex-shrink-0" />
              <span>{error}</span>
            </div>
          )}

          {/* Title */}
          <div>
            <label className="block font-medium text-[#202020] mb-1">
              Task Title <span className="text-red-500">*</span>
            </label>
            <input
              type="text"
              required
              value={title}
              onChange={e => setTitle(e.target.value)}
              placeholder="e.g. Implement Topological Sort & Cycle Invariant Checks"
              className="w-full px-3 py-2 text-xs border border-[#DEDEDA] rounded-md bg-white focus:outline-none focus:border-[#202020]"
            />
          </div>

          {/* Description */}
          <div>
            <label className="block font-medium text-[#202020] mb-1">Description</label>
            <textarea
              rows={2}
              value={description}
              onChange={e => setDescription(e.target.value)}
              placeholder="Provide technical context or acceptance criteria..."
              className="w-full px-3 py-2 text-xs border border-[#DEDEDA] rounded-md bg-white focus:outline-none focus:border-[#202020]"
            />
          </div>

          {/* Row: Duration & Priority */}
          <div className="grid grid-cols-2 gap-4">
            <div>
              <label className="block font-medium text-[#202020] mb-1">
                Estimated Duration
              </label>
              <div className="flex items-center gap-2">
                <div className="flex items-center gap-1 flex-1">
                  <input
                    type="number"
                    min="0"
                    max="24"
                    value={durationHours}
                    onChange={e => setDurationHours(Math.max(0, parseInt(e.target.value, 10) || 0))}
                    className="w-full px-2.5 py-1.5 border border-[#DEDEDA] rounded-md text-xs text-center"
                  />
                  <span className="text-[#777773]">hrs</span>
                </div>
                <div className="flex items-center gap-1 flex-1">
                  <input
                    type="number"
                    min="0"
                    max="59"
                    step="5"
                    value={durationMinutes}
                    onChange={e => setDurationMinutes(Math.max(0, Math.min(59, parseInt(e.target.value, 10) || 0)))}
                    className="w-full px-2.5 py-1.5 border border-[#DEDEDA] rounded-md text-xs text-center"
                  />
                  <span className="text-[#777773]">mins</span>
                </div>
              </div>
            </div>

            <div>
              <label className="block font-medium text-[#202020] mb-1">Priority</label>
              <select
                value={priority}
                onChange={e => setPriority(e.target.value as TaskPriority)}
                className="w-full px-3 py-1.5 border border-[#DEDEDA] rounded-md bg-white text-xs"
              >
                <option value="High">High Priority</option>
                <option value="Medium">Medium Priority</option>
                <option value="Low">Low Priority</option>
              </select>
            </div>
          </div>

          {/* Row: Deadline & Category */}
          <div className="grid grid-cols-2 gap-4">
            <div>
              <label className="block font-medium text-[#202020] mb-1">Deadline Date &amp; Time</label>
              <input
                type="datetime-local"
                value={deadline}
                onChange={e => setDeadline(e.target.value)}
                className="w-full px-3 py-1.5 border border-[#DEDEDA] rounded-md text-xs"
              />
            </div>
            <div>
              <label className="block font-medium text-[#202020] mb-1">Category</label>
              <input
                type="text"
                value={category}
                onChange={e => setCategory(e.target.value)}
                placeholder="e.g. Academic, Work, Research"
                className="w-full px-3 py-1.5 border border-[#DEDEDA] rounded-md text-xs"
              />
            </div>
          </div>

          {/* Row: Earliest start & Latest completion */}
          <div className="grid grid-cols-2 gap-4">
            <div>
              <label className="block font-medium text-[#202020] mb-1">Earliest Start Window</label>
              <input
                type="datetime-local"
                value={earliestStartDate}
                onChange={e => setEarliestStartDate(e.target.value)}
                className="w-full px-3 py-1.5 border border-[#DEDEDA] rounded-md text-xs"
              />
            </div>
            <div>
              <label className="block font-medium text-[#202020] mb-1">Latest Acceptable Completion</label>
              <input
                type="datetime-local"
                value={latestAcceptableCompletionDate}
                onChange={e => setLatestAcceptableCompletionDate(e.target.value)}
                className="w-full px-3 py-1.5 border border-[#DEDEDA] rounded-md text-xs"
              />
            </div>
          </div>

          {/* Allow Task Splitting */}
          <div className="flex items-center justify-between p-3 border border-[#DEDEDA] rounded-md bg-[#FAFAF9]">
            <div>
              <div className="font-medium text-[#202020]">Allow Task Splitting</div>
              <div className="text-[11px] text-[#777773]">
                Permits the C++ scheduling engine to break this task across disjoint gaps if needed.
              </div>
            </div>
            <label className="relative inline-flex items-center cursor-pointer">
              <input
                type="checkbox"
                checked={allowSplitting}
                onChange={e => setAllowSplitting(e.target.checked)}
                className="sr-only peer"
              />
              <div className="w-8 h-4.5 bg-[#DEDEDA] peer-focus:outline-none rounded-full peer peer-checked:after:translate-x-full peer-checked:after:border-white after:content-[''] after:absolute after:top-[2px] after:left-[2px] after:bg-white after:border-gray-300 after:border after:rounded-full after:h-3.5 after:w-3.5 after:transition-all peer-checked:bg-[#202020]"></div>
            </label>
          </div>

          {/* Dependencies Multi-Select */}
          <div>
            <label className="block font-medium text-[#202020] mb-1">
              Prerequisite Dependencies ({dependencies.length} selected)
            </label>
            <div className="max-h-32 overflow-y-auto border border-[#DEDEDA] rounded-md p-2 space-y-1 bg-white">
              {availableDependencies.length === 0 ? (
                <div className="text-[11px] text-[#777773] py-2 text-center">
                  No other tasks available for dependency linkage.
                </div>
              ) : (
                availableDependencies.map(t => {
                  const isChecked = dependencies.includes(t.id);
                  return (
                    <label
                      key={t.id}
                      className={`flex items-center gap-2 p-1.5 rounded cursor-pointer transition-colors ${
                        isChecked ? 'bg-[#EFEFED] font-medium' : 'hover:bg-[#F7F7F5]'
                      }`}
                    >
                      <input
                        type="checkbox"
                        checked={isChecked}
                        onChange={() => toggleDependency(t.id)}
                        className="rounded border-[#DEDEDA] text-[#202020] focus:ring-0"
                      />
                      <span className="truncate flex-1">{t.title}</span>
                      <span className="text-[10px] text-[#777773] uppercase">{t.status}</span>
                    </label>
                  );
                })
              )}
            </div>
          </div>

          {/* Form Actions */}
          <div className="flex items-center justify-end gap-2 pt-3 border-t border-[#DEDEDA]">
            <button
              type="button"
              onClick={handleClose}
              className="px-3 py-1.5 text-xs text-[#555552] hover:text-[#202020] hover:bg-[#E8E8E5] rounded-md transition-colors"
            >
              Cancel
            </button>
            <button
              type="submit"
              className="px-4 py-1.5 text-xs font-medium bg-[#202020] hover:bg-[#333330] text-white rounded-md transition-colors"
            >
              {isEditing ? 'Save Changes' : 'Create Task'}
            </button>
          </div>
        </form>
      </div>
    </div>
  );
};
