import React from 'react';
import { useApp } from '../../context/AppContext';
import { X, CheckCircle2, AlertTriangle, AlertCircle, Info } from 'lucide-react';

export const ToastContainer: React.FC = () => {
  const { toasts, removeToast } = useApp();

  if (toasts.length === 0) return null;

  return (
    <div className="fixed bottom-4 right-4 z-50 flex flex-col gap-2 max-w-sm w-full pointer-events-none">
      {toasts.map(toast => {
        let Icon = Info;
        let borderClass = 'border-[#DEDEDA]';
        let bgClass = 'bg-white';

        if (toast.type === 'success') {
          Icon = CheckCircle2;
          borderClass = 'border-[#BBF7D0]';
          bgClass = 'bg-[#FAFCFA]';
        } else if (toast.type === 'warning') {
          Icon = AlertTriangle;
          borderClass = 'border-[#FDE68A]';
          bgClass = 'bg-[#FFFDF7]';
        } else if (toast.type === 'error') {
          Icon = AlertCircle;
          borderClass = 'border-[#FECACA]';
          bgClass = 'bg-[#FEFBFB]';
        }

        return (
          <div
            key={toast.id}
            className={`pointer-events-auto flex items-start gap-2.5 p-3 rounded-md border shadow-sm ${bgClass} ${borderClass} transition-all`}
          >
            <Icon className="w-4 h-4 mt-0.5 text-[#555552] flex-shrink-0" />
            <div className="flex-1 min-w-0">
              <div className="text-xs font-semibold text-[#202020]">{toast.title}</div>
              {toast.description && (
                <div className="text-[12px] text-[#777773] mt-0.5 leading-snug">{toast.description}</div>
              )}
            </div>
            <button
              onClick={() => removeToast(toast.id)}
              className="text-[#999995] hover:text-[#202020] p-0.5 transition-colors"
              aria-label="Dismiss toast"
            >
              <X className="w-3.5 h-3.5" />
            </button>
          </div>
        );
      })}
    </div>
  );
};
