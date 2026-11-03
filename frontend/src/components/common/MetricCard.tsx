import React from 'react';
import { LucideIcon } from 'lucide-react';

interface MetricCardProps {
  label: string;
  value: number | string;
  subtext?: string;
  icon?: LucideIcon;
  trend?: string;
  alert?: boolean;
}

export const MetricCard: React.FC<MetricCardProps> = ({
  label,
  value,
  subtext,
  icon: Icon,
  trend,
  alert = false
}) => {
  return (
    <div
      className={`bg-white border rounded-md p-4 transition-colors ${
        alert ? 'border-[#E5A5A5] bg-[#FDFCFB]' : 'border-[#DEDEDA]'
      }`}
    >
      <div className="flex items-center justify-between text-xs text-[#777773] mb-1">
        <span className="font-medium uppercase tracking-wider text-[11px]">{label}</span>
        {Icon && <Icon className="w-3.5 h-3.5 text-[#888884]" />}
      </div>
      <div className="flex items-baseline gap-2">
        <span className="text-2xl font-semibold text-[#202020] tracking-tight">{value}</span>
        {trend && (
          <span className="text-[11px] text-[#777773]">{trend}</span>
        )}
      </div>
      {subtext && (
        <div className="mt-1.5 text-xs text-[#777773] truncate">
          {subtext}
        </div>
      )}
    </div>
  );
};
