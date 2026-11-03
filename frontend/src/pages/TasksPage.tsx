import React from 'react';
import { PageHeader } from '../components/common/PageHeader';
import { TaskTable } from '../components/tasks/TaskTable';
import { useApp } from '../context/AppContext';
import { Plus } from 'lucide-react';

export const TasksPage: React.FC = () => {
  const { setActiveModal, setSelectedTask } = useApp();

  return (
    <div className="space-y-6">
      <PageHeader
        heading="Tasks"
        description="Manage priorities, deadlines and workload."
      >
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
      </PageHeader>

      <TaskTable />
    </div>
  );
};
