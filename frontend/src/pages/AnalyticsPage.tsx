import React from 'react';
import { PageHeader } from '../components/common/PageHeader';
import { AnalyticsDashboard } from '../components/analytics/AnalyticsDashboard';
import { BarChart3 } from 'lucide-react';

export const AnalyticsPage: React.FC = () => {
  return (
    <div className="space-y-6">
      <PageHeader
        heading="Analytics"
        description="Quantitative insights on scheduling efficiency, task completion rates, and workload balance."
      >
        <span className="text-xs font-mono text-[#777773] bg-[#EFEFED] px-3 py-1.5 rounded-md border border-[#DEDEDA]">
          Reporting Period: Current Sprint
        </span>
      </PageHeader>

      <AnalyticsDashboard />
    </div>
  );
};
