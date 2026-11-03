import React from 'react';

interface PageHeaderProps {
  heading: string;
  description?: string;
  children?: React.ReactNode;
}

export const PageHeader: React.FC<PageHeaderProps> = ({
  heading,
  description,
  children
}) => {
  return (
    <div className="flex flex-col sm:flex-row sm:items-center sm:justify-between gap-4 pb-5 border-b border-[#DEDEDA] mb-6">
      <div>
        <h1 className="text-2xl sm:text-[26px] font-semibold text-[#202020] tracking-tight">
          {heading}
        </h1>
        {description && (
          <p className="text-[13px] text-[#777773] mt-1 leading-normal">
            {description}
          </p>
        )}
      </div>
      {children && (
        <div className="flex items-center gap-2.5 flex-shrink-0">
          {children}
        </div>
      )}
    </div>
  );
};
