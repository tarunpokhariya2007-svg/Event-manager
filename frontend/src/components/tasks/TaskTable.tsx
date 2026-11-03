import React, { useState, useMemo } from 'react';
import { useApp } from '../../context/AppContext';
import { Task, TaskPriority, TaskStatus, TaskSortField, SortOrder } from '../../types/task';
import {
  Search,
  Plus,
  Trash2,
  Edit2,
  CheckCircle2,
  Circle,
  Clock,
  ArrowUpDown,
  Filter,
  CheckSquare,
  AlertCircle
} from 'lucide-react';
import { PriorityBadge } from '../common/PriorityBadge';
import { StatusBadge } from '../common/StatusBadge';
import { formatDuration, formatDateReadable } from '../../utils/timeUtils';
import { ConfirmDialog } from '../common/ConfirmDialog';
import { EmptyState } from '../common/EmptyState';

export const TaskTable: React.FC = () => {
  const {
    tasks,
    deleteTask,
    toggleTaskComplete,
    setActiveModal,
    setSelectedTask
  } = useApp();

  const [search, setSearch] = useState('');
  const [priorityFilter, setPriorityFilter] = useState<'All' | TaskPriority>('All');
  const [statusFilter, setStatusFilter] = useState<'All' | TaskStatus>('All');
  const [sortField, setSortField] = useState<TaskSortField>('deadline');
  const [sortOrder, setSortOrder] = useState<SortOrder>('asc');

  const [taskToDelete, setTaskToDelete] = useState<Task | null>(null);

  // Summary counts
  const summaryCounts = useMemo(() => {
    return {
      all: tasks.length,
      pending: tasks.filter(t => t.status === 'Pending' || t.status === 'In Progress').length,
      completed: tasks.filter(t => t.status === 'Completed').length,
      overdue: tasks.filter(t => t.status === 'Overdue').length
    };
  }, [tasks]);

  // Filtering & Sorting
  const filteredAndSortedTasks = useMemo(() => {
    return tasks
      .filter(task => {
        const matchesSearch =
          task.title.toLowerCase().includes(search.toLowerCase()) ||
          task.description?.toLowerCase().includes(search.toLowerCase()) ||
          task.category?.toLowerCase().includes(search.toLowerCase());

        const matchesPriority =
          priorityFilter === 'All' || task.priority === priorityFilter;

        const matchesStatus =
          statusFilter === 'All' || task.status === statusFilter;

        return matchesSearch && matchesPriority && matchesStatus;
      })
      .sort((a, b) => {
        let comp = 0;
        if (sortField === 'deadline') {
          comp = new Date(a.deadline).getTime() - new Date(b.deadline).getTime();
        } else if (sortField === 'priority') {
          const rank: Record<TaskPriority, number> = { High: 3, Medium: 2, Low: 1 };
          comp = rank[b.priority] - rank[a.priority];
        } else if (sortField === 'estimatedDuration') {
          comp = a.estimatedDuration - b.estimatedDuration;
        } else if (sortField === 'title') {
          comp = a.title.localeCompare(b.title);
        }
        return sortOrder === 'asc' ? comp : -comp;
      });
  }, [tasks, search, priorityFilter, statusFilter, sortField, sortOrder]);

  const toggleSort = (field: TaskSortField) => {
    if (sortField === field) {
      setSortOrder(prev => (prev === 'asc' ? 'desc' : 'asc'));
    } else {
      setSortField(field);
      setSortOrder('asc');
    }
  };

  const handleEdit = (task: Task) => {
    setSelectedTask(task);
    setActiveModal('edit_task');
  };

  const confirmDelete = () => {
    if (taskToDelete) {
      deleteTask(taskToDelete.id);
      setTaskToDelete(null);
    }
  };

  return (
    <div className="space-y-4">
      {/* Summary KPI Strip */}
      <div className="grid grid-cols-2 sm:grid-cols-4 gap-3">
        <div
          onClick={() => setStatusFilter('All')}
          className={`p-3 bg-white border rounded-md cursor-pointer transition-colors ${
            statusFilter === 'All' ? 'border-[#202020]' : 'border-[#DEDEDA] hover:bg-[#FAFAF9]'
          }`}
        >
          <div className="text-[11px] font-medium text-[#777773] uppercase tracking-wider">All Tasks</div>
          <div className="text-xl font-semibold text-[#202020] mt-0.5">{summaryCounts.all}</div>
        </div>
        <div
          onClick={() => setStatusFilter('Pending')}
          className={`p-3 bg-white border rounded-md cursor-pointer transition-colors ${
            statusFilter === 'Pending' ? 'border-[#202020]' : 'border-[#DEDEDA] hover:bg-[#FAFAF9]'
          }`}
        >
          <div className="text-[11px] font-medium text-[#777773] uppercase tracking-wider">Pending</div>
          <div className="text-xl font-semibold text-[#202020] mt-0.5">{summaryCounts.pending}</div>
        </div>
        <div
          onClick={() => setStatusFilter('Completed')}
          className={`p-3 bg-white border rounded-md cursor-pointer transition-colors ${
            statusFilter === 'Completed' ? 'border-[#202020]' : 'border-[#DEDEDA] hover:bg-[#FAFAF9]'
          }`}
        >
          <div className="text-[11px] font-medium text-[#777773] uppercase tracking-wider">Completed</div>
          <div className="text-xl font-semibold text-[#202020] mt-0.5">{summaryCounts.completed}</div>
        </div>
        <div
          onClick={() => setStatusFilter('Overdue')}
          className={`p-3 bg-white border rounded-md cursor-pointer transition-colors ${
            statusFilter === 'Overdue' ? 'border-[#202020]' : 'border-[#DEDEDA] hover:bg-[#FAFAF9]'
          }`}
        >
          <div className="text-[11px] font-medium text-[#777773] uppercase tracking-wider">Overdue</div>
          <div className="text-xl font-semibold text-[#202020] mt-0.5">{summaryCounts.overdue}</div>
        </div>
      </div>

      {/* Table Toolbar */}
      <div className="bg-white border border-[#DEDEDA] rounded-lg shadow-xs overflow-hidden">
        <div className="p-3.5 border-b border-[#DEDEDA] flex flex-col md:flex-row md:items-center justify-between gap-3 bg-[#FAFAF9]">
          {/* Search */}
          <div className="relative flex-1 max-w-sm">
            <Search className="w-3.5 h-3.5 text-[#777773] absolute left-3 top-2.5" />
            <input
              type="text"
              value={search}
              onChange={e => setSearch(e.target.value)}
              placeholder="Filter tasks by name, category, or notes..."
              className="w-full pl-8 pr-3 py-1.5 text-xs border border-[#DEDEDA] rounded-md bg-white focus:outline-none focus:border-[#202020]"
            />
          </div>

          {/* Filters & Actions */}
          <div className="flex items-center gap-2 flex-wrap">
            {/* Priority Filter */}
            <select
              value={priorityFilter}
              onChange={e => setPriorityFilter(e.target.value as any)}
              className="px-2.5 py-1.5 text-xs border border-[#DEDEDA] rounded-md bg-white text-[#202020]"
            >
              <option value="All">All Priorities</option>
              <option value="High">High Priority</option>
              <option value="Medium">Medium Priority</option>
              <option value="Low">Low Priority</option>
            </select>

            {/* Status Filter */}
            <select
              value={statusFilter}
              onChange={e => setStatusFilter(e.target.value as any)}
              className="px-2.5 py-1.5 text-xs border border-[#DEDEDA] rounded-md bg-white text-[#202020]"
            >
              <option value="All">All Statuses</option>
              <option value="Pending">Pending</option>
              <option value="In Progress">In Progress</option>
              <option value="Completed">Completed</option>
              <option value="Blocked">Blocked</option>
              <option value="Overdue">Overdue</option>
            </select>

            {/* Add Task Button */}
            <button
              onClick={() => {
                setSelectedTask(null);
                setActiveModal('add_task');
              }}
              className="inline-flex items-center gap-1.5 px-3 py-1.5 bg-[#202020] hover:bg-[#333330] text-white text-xs font-medium rounded-md transition-colors"
            >
              <Plus className="w-3.5 h-3.5" />
              <span>Add Task</span>
            </button>
          </div>
        </div>

        {/* Tasks Table */}
        {filteredAndSortedTasks.length === 0 ? (
          <div className="p-8">
            <EmptyState
              title="No tasks match the filter criteria"
              description="Adjust your search term, priority, or status filter to see other tasks."
              actionText="Add New Task"
              onAction={() => {
                setSearch('');
                setPriorityFilter('All');
                setStatusFilter('All');
                setActiveModal('add_task');
              }}
            />
          </div>
        ) : (
          <div className="overflow-x-auto">
            <table className="w-full text-left text-xs">
              <thead className="bg-[#FAFAF9] border-b border-[#DEDEDA] text-[11px] text-[#777773] uppercase tracking-wider select-none font-medium">
                <tr>
                  <th className="py-2.5 px-4 w-10 text-center">Done</th>
                  <th
                    className="py-2.5 px-4 cursor-pointer hover:text-[#202020]"
                    onClick={() => toggleSort('title')}
                  >
                    <div className="flex items-center gap-1">
                      <span>Task</span>
                      <ArrowUpDown className="w-3 h-3" />
                    </div>
                  </th>
                  <th
                    className="py-2.5 px-4 cursor-pointer hover:text-[#202020]"
                    onClick={() => toggleSort('priority')}
                  >
                    <div className="flex items-center gap-1">
                      <span>Priority</span>
                      <ArrowUpDown className="w-3 h-3" />
                    </div>
                  </th>
                  <th
                    className="py-2.5 px-4 cursor-pointer hover:text-[#202020]"
                    onClick={() => toggleSort('estimatedDuration')}
                  >
                    <div className="flex items-center gap-1">
                      <span>Est. Duration</span>
                      <ArrowUpDown className="w-3 h-3" />
                    </div>
                  </th>
                  <th
                    className="py-2.5 px-4 cursor-pointer hover:text-[#202020]"
                    onClick={() => toggleSort('deadline')}
                  >
                    <div className="flex items-center gap-1">
                      <span>Deadline</span>
                      <ArrowUpDown className="w-3 h-3" />
                    </div>
                  </th>
                  <th className="py-2.5 px-4">Status</th>
                  <th className="py-2.5 px-4 text-right">Actions</th>
                </tr>
              </thead>
              <tbody className="divide-y divide-[#EFEFED]">
                {filteredAndSortedTasks.map(task => {
                  const isDone = task.status === 'Completed';

                  return (
                    <tr
                      key={task.id}
                      className={`hover:bg-[#F7F7F5] transition-colors group ${
                        isDone ? 'bg-[#FAFAFA]' : ''
                      }`}
                    >
                      {/* Checkbox */}
                      <td className="py-3 px-4 text-center">
                        <button
                          onClick={() => toggleTaskComplete(task.id)}
                          className="text-[#777773] hover:text-[#202020] transition-colors"
                          aria-label={isDone ? 'Mark incomplete' : 'Mark complete'}
                        >
                          {isDone ? (
                            <CheckCircle2 className="w-4 h-4 text-[#166534]" />
                          ) : (
                            <Circle className="w-4 h-4" />
                          )}
                        </button>
                      </td>

                      {/* Title & Details */}
                      <td className="py-3 px-4 max-w-sm">
                        <div
                          className={`font-medium ${
                            isDone ? 'line-through text-[#888884]' : 'text-[#202020]'
                          }`}
                        >
                          {task.title}
                        </div>
                        {task.description && (
                          <div className="text-[11px] text-[#777773] truncate mt-0.5">
                            {task.description}
                          </div>
                        )}
                        {task.dependencies && task.dependencies.length > 0 && (
                          <div className="text-[10px] text-[#777773] mt-1 flex items-center gap-1">
                            <span>Prereq: {task.dependencies.length} task(s)</span>
                          </div>
                        )}
                      </td>

                      {/* Priority */}
                      <td className="py-3 px-4 whitespace-nowrap">
                        <PriorityBadge priority={task.priority} size="sm" />
                      </td>

                      {/* Duration */}
                      <td className="py-3 px-4 whitespace-nowrap text-[#555552] font-mono">
                        {formatDuration(task.estimatedDuration)}
                        {task.allowSplitting && (
                          <span
                            title="Splitting allowed by scheduler"
                            className="ml-1 text-[10px] text-[#888884] font-sans"
                          >
                            (split)
                          </span>
                        )}
                      </td>

                      {/* Deadline */}
                      <td className="py-3 px-4 whitespace-nowrap text-[#555552]">
                        <div>{formatDateReadable(task.deadline.split('T')[0])}</div>
                        <div className="text-[11px] text-[#777773]">
                          {task.deadline.split('T')[1] || ''}
                        </div>
                      </td>

                      {/* Status */}
                      <td className="py-3 px-4 whitespace-nowrap">
                        <StatusBadge status={task.status} size="sm" />
                      </td>

                      {/* Actions */}
                      <td className="py-3 px-4 text-right whitespace-nowrap">
                        <div className="flex items-center justify-end gap-1 opacity-80 group-hover:opacity-100 transition-opacity">
                          <button
                            onClick={() => handleEdit(task)}
                            className="p-1 text-[#777773] hover:text-[#202020] hover:bg-[#E8E8E5] rounded transition-colors"
                            title="Edit task"
                          >
                            <Edit2 className="w-3.5 h-3.5" />
                          </button>
                          <button
                            onClick={() => setTaskToDelete(task)}
                            className="p-1 text-[#777773] hover:text-[#991B1B] hover:bg-[#FEF2F2] rounded transition-colors"
                            title="Delete task"
                          >
                            <Trash2 className="w-3.5 h-3.5" />
                          </button>
                        </div>
                      </td>
                    </tr>
                  );
                })}
              </tbody>
            </table>
          </div>
        )}
      </div>

      {/* Delete Confirmation Modal */}
      <ConfirmDialog
        isOpen={Boolean(taskToDelete)}
        title="Delete Task"
        message={`Are you sure you want to delete "${taskToDelete?.title}"? This will also remove any dependency linkages from other tasks.`}
        confirmText="Delete Task"
        onConfirm={confirmDelete}
        onCancel={() => setTaskToDelete(null)}
      />
    </div>
  );
};
