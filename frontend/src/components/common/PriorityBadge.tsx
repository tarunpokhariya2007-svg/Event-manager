import React from 'react';
import { TaskPriority } from '../../types/task';
import { EventPriority } from '../../types/event';

interface PriorityBadgeProps {
  priority: TaskPriority | EventPriority;
  size?: 'sm' | 'md';
}

export const PriorityBadge: React.FC<PriorityBadgeProps> = ({ priority, size = 'md' }) => {
  const sizeClasses = size === 'sm' ? 'px-1.5 py-0.5 text-[11px]' : 'px-2 py-0.5 text-xs';

  // Strict monochromatic styling as required:
  // No bright decorative colors; subtle neutral tones with delicate borders
  let styleClasses = 'bg-[#F2F2EF] text-[#333330] border-[#DEDEDA]';
  
  if (priority === 'High') {
    styleClasses = 'bg-[#202020] text-[#FFFFFF] border-[#202020] font-medium';
  } else if (priority === 'Medium') {
    styleClasses = 'bg-[#EAEAE6] text-[#242424] border-[#D5D5D0]';
  } else {
    styleClasses = 'bg-[#F7F7F5] text-[#777773] border-[#E2E2DE]';
  }

  return (
    <span
      className={`inline-flex items-center rounded border font-normal uppercase tracking-wider ${sizeClasses} ${styleClasses}`}
    >
      {priority}
    </span>
  );
};
