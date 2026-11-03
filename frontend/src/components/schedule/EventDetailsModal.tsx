import React, { useState } from 'react';
import { useApp } from '../../context/AppContext';
import { X, Calendar, Clock, MapPin, Tag, Flag, AlertCircle, Edit, Trash2 } from 'lucide-react';
import { formatTime12h, formatDateReadable } from '../../utils/timeUtils';
import { PriorityBadge } from '../common/PriorityBadge';
import { ConfirmDialog } from '../common/ConfirmDialog';

export const EventDetailsModal: React.FC = () => {
  const { activeModal, setActiveModal, selectedEvent, setSelectedEvent, deleteEvent } = useApp();
  const [showConfirmDelete, setShowConfirmDelete] = useState(false);

  if (activeModal !== 'event_details' || !selectedEvent) return null;

  const handleClose = () => {
    setActiveModal(null);
    setSelectedEvent(null);
  };

  const handleEdit = () => {
    setActiveModal('edit_event');
  };

  const handleDelete = () => {
    deleteEvent(selectedEvent.id);
    setShowConfirmDelete(false);
    handleClose();
  };

  return (
    <>
      <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/40 backdrop-blur-[1px] p-4">
        <div
          className="w-full max-w-md bg-white border border-[#DEDEDA] rounded-lg shadow-xl overflow-hidden flex flex-col"
          onClick={e => e.stopPropagation()}
        >
          {/* Header */}
          <div className="flex items-center justify-between px-5 py-3.5 border-b border-[#DEDEDA] bg-[#FAFAF9]">
            <div className="flex items-center gap-2">
              <span className="text-xs font-medium text-[#777773] uppercase tracking-wider">
                Event Details
              </span>
            </div>
            <button
              onClick={handleClose}
              className="text-[#777773] hover:text-[#202020] p-1 rounded hover:bg-[#E8E8E5] transition-colors"
            >
              <X className="w-4 h-4" />
            </button>
          </div>

          {/* Content */}
          <div className="p-5 space-y-4 text-xs">
            <div>
              <div className="flex items-center gap-2 mb-1.5">
                <PriorityBadge priority={selectedEvent.priority} size="sm" />
                <span className="px-1.5 py-0.5 text-[11px] rounded bg-[#EFEFED] text-[#555552] border border-[#DEDEDA]">
                  {selectedEvent.category}
                </span>
                <span className="px-1.5 py-0.5 text-[11px] rounded bg-[#EFEFED] text-[#555552] border border-[#DEDEDA]">
                  {selectedEvent.flexibility}
                </span>
              </div>
              <h2 className="text-base font-semibold text-[#202020] leading-snug">
                {selectedEvent.title}
              </h2>
            </div>

            {selectedEvent.description && (
              <p className="text-[#555552] leading-relaxed bg-[#F7F7F5] p-3 rounded-md border border-[#E8E8E5]">
                {selectedEvent.description}
              </p>
            )}

            <div className="space-y-2.5 pt-1 border-t border-[#DEDEDA] text-[#555552]">
              <div className="flex items-center gap-2.5">
                <Calendar className="w-4 h-4 text-[#777773]" />
                <span className="font-medium text-[#202020]">
                  {formatDateReadable(selectedEvent.date)}
                </span>
              </div>
              <div className="flex items-center gap-2.5">
                <Clock className="w-4 h-4 text-[#777773]" />
                <span>
                  {formatTime12h(selectedEvent.startTime)} – {formatTime12h(selectedEvent.endTime)}
                </span>
              </div>
              {selectedEvent.location && (
                <div className="flex items-center gap-2.5">
                  <MapPin className="w-4 h-4 text-[#777773]" />
                  <span>{selectedEvent.location}</span>
                </div>
              )}
            </div>

            {/* Actions */}
            <div className="flex items-center justify-between pt-4 border-t border-[#DEDEDA]">
              <button
                type="button"
                onClick={() => setShowConfirmDelete(true)}
                className="inline-flex items-center gap-1.5 px-3 py-1.5 text-xs text-[#991B1B] hover:bg-[#FEF2F2] rounded-md transition-colors"
              >
                <Trash2 className="w-3.5 h-3.5" />
                <span>Delete</span>
              </button>

              <div className="flex items-center gap-2">
                <button
                  type="button"
                  onClick={handleClose}
                  className="px-3 py-1.5 text-xs text-[#555552] hover:text-[#202020] hover:bg-[#E8E8E5] rounded-md transition-colors"
                >
                  Close
                </button>
                <button
                  type="button"
                  onClick={handleEdit}
                  className="inline-flex items-center gap-1.5 px-3.5 py-1.5 text-xs font-medium bg-[#202020] text-white hover:bg-[#333330] rounded-md transition-colors"
                >
                  <Edit className="w-3.5 h-3.5" />
                  <span>Edit Event</span>
                </button>
              </div>
            </div>
          </div>
        </div>
      </div>

      <ConfirmDialog
        isOpen={showConfirmDelete}
        title="Delete Event"
        message={`Are you sure you want to delete "${selectedEvent.title}"? This cannot be undone.`}
        confirmText="Delete Event"
        onConfirm={handleDelete}
        onCancel={() => setShowConfirmDelete(false)}
      />
    </>
  );
};
