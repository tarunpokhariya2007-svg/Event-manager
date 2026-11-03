import React from 'react';
import { PageHeader } from '../components/common/PageHeader';
import { DependencyGraph } from '../components/dependencies/DependencyGraph';
import { useApp } from '../context/AppContext';
import { GitBranch, Plus } from 'lucide-react';

export const DependenciesPage: React.FC = () => {
  const { setActiveModal, setSelectedTask } = useApp();

  return (
    <div className="space-y-6">
      <PageHeader
        heading="Task Dependencies"
        description="Visualize prerequisite directed acyclic graphs (DAGs), track unblocked execution flows, and detect circular loops."
      >
        <button
          onClick={() => {
            setSelectedTask(null);
            setActiveModal('add_task');
          }}
          className="inline-flex items-center gap-1.5 px-3 py-1.5 bg-[#202020] hover:bg-[#333330] text-white text-xs font-medium rounded-md transition-colors"
        >
          <Plus className="w-3.5 h-3.5" />
          <span>Add Task with Dependencies</span>
        </button>
      </PageHeader>

      <DependencyGraph />
    </div>
  );
};
