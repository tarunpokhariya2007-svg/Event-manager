import React from 'react';
import { LucideIcon, Inbox } from 'lucide-react';

interface EmptyStateProps {
  title: string;
  description: string;
  icon?: LucideIcon;
  actionText?: string;
  onAction?: () => void;
}

export const EmptyState: React.FC<EmptyStateProps> = ({
  title,
  description,
  icon: Icon = Inbox,
  actionText,
  onAction
}) => {
  return (
    <div className="flex flex-col items-center justify-center p-10 text-center border border-dashed border-[#DEDEDA] rounded-md bg-[#FAFAF9]">
      <div className="w-10 h-10 rounded-md bg-[#EFEFED] flex items-center justify-center text-[#777773] mb-3">
        <Icon className="w-5 h-5 stroke-[1.75]" />
      </div>
      <h3 className="text-sm font-medium text-[#202020] mb-1">{title}</h3>
      <p className="text-xs text-[#777773] max-w-sm mb-4 leading-relaxed">{description}</p>
      {actionText && onAction && (
        <button
          onClick={onAction}
          className="inline-flex items-center gap-1.5 px-3 py-1.5 text-xs font-medium text-[#202020] bg-white border border-[#DEDEDA] rounded-md hover:bg-[#E8E8E5] transition-colors"
        >
          {actionText}
        </button>
      )}
    </div>
  );
};
