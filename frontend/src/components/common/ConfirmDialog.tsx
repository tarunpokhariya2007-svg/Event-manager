import React from 'react';
import { AlertTriangle, X } from 'lucide-react';

interface ConfirmDialogProps {
  isOpen: boolean;
  title: string;
  message: string;
  confirmText?: string;
  cancelText?: string;
  onConfirm: () => void;
  onCancel: () => void;
}

export const ConfirmDialog: React.FC<ConfirmDialogProps> = ({
  isOpen,
  title,
  message,
  confirmText = 'Confirm Delete',
  cancelText = 'Cancel',
  onConfirm,
  onCancel
}) => {
  if (!isOpen) return null;

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/40 backdrop-blur-[1px] p-4">
      <div
        className="w-full max-w-sm bg-white border border-[#DEDEDA] rounded-lg shadow-xl overflow-hidden p-5"
        onClick={e => e.stopPropagation()}
      >
        <div className="flex items-start gap-3">
          <div className="w-8 h-8 rounded-full bg-[#FFFBEB] border border-[#FDE68A] flex items-center justify-center text-[#92400E] flex-shrink-0">
            <AlertTriangle className="w-4 h-4" />
          </div>
          <div className="flex-1 min-w-0">
            <h3 className="text-sm font-semibold text-[#202020]">{title}</h3>
            <p className="text-xs text-[#777773] mt-1 leading-relaxed">{message}</p>
          </div>
        </div>

        <div className="flex items-center justify-end gap-2 mt-5 pt-3 border-t border-[#DEDEDA]">
          <button
            type="button"
            onClick={onCancel}
            className="px-3 py-1.5 text-xs text-[#555552] hover:text-[#202020] hover:bg-[#E8E8E5] rounded-md transition-colors"
          >
            {cancelText}
          </button>
          <button
            type="button"
            onClick={onConfirm}
            className="px-3.5 py-1.5 text-xs font-medium bg-[#991B1B] text-white hover:bg-[#7F1D1D] rounded-md transition-colors"
          >
            {confirmText}
          </button>
        </div>
      </div>
    </div>
  );
};
