import React from 'react';
import { TaskStatus } from '../../types/task';

interface StatusBadgeProps {
  status: TaskStatus | 'Fixed Event' | 'Scheduled Task' | 'Recuperation Window';
  size?: 'sm' | 'md';
}

export const StatusBadge: React.FC<StatusBadgeProps> = ({ status, size = 'md' }) => {
  const sizeClasses = size === 'sm' ? 'px-1.5 py-0.5 text-[11px]' : 'px-2 py-0.5 text-xs';

  let colorClasses = 'bg-[#F2F2EF] text-[#555552] border-[#DEDEDA]';

  switch (status) {
    case 'Completed':
      // Muted green strictly for completion
      colorClasses = 'bg-[#F0FDF4] text-[#166534] border-[#BBF7D0]';
      break;
    case 'In Progress':
      // Neutral active state
      colorClasses = 'bg-[#E3E3DF] text-[#1E1E1E] border-[#C8C8C2] font-medium';
      break;
    case 'Overdue':
      // Muted red strictly for overdue warning
      colorClasses = 'bg-[#FEF2F2] text-[#991B1B] border-[#FECACA]';
      break;
    case 'Blocked':
      // Muted amber strictly for blocked dependency
      colorClasses = 'bg-[#FFFBEB] text-[#92400E] border-[#FDE68A]';
      break;
    case 'Pending':
      colorClasses = 'bg-[#F7F7F5] text-[#666662] border-[#DEDEDA]';
      break;
    case 'Fixed Event':
      colorClasses = 'bg-[#202020] text-[#FFFFFF] border-[#202020] font-medium';
      break;
    case 'Scheduled Task':
      colorClasses = 'bg-[#EFEFED] text-[#222220] border-[#D5D5D0]';
      break;
    case 'Recuperation Window':
      colorClasses = 'bg-[#F9F9F8] text-[#777773] border-dashed border-[#DEDEDA]';
      break;
  }

  return (
    <span
      className={`inline-flex items-center rounded border font-medium ${sizeClasses} ${colorClasses}`}
    >
      {status}
    </span>
  );
};
