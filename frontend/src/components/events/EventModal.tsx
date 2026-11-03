import React, { useState, useEffect } from 'react';
import { useApp } from '../../context/AppContext';
import { CalendarEvent, EventCategory, EventFlexibility, EventPriority } from '../../types/event';
import { X, AlertCircle } from 'lucide-react';
import { getTodayDateString, parseMinutes } from '../../utils/timeUtils';

export const EventModal: React.FC = () => {
  const { activeModal, setActiveModal, selectedEvent, setSelectedEvent, addEvent, updateEvent } = useApp();

  const isEditing = activeModal === 'edit_event';
  const isOpen = activeModal === 'add_event' || activeModal === 'edit_event';

  const today = getTodayDateString();

  const [title, setTitle] = useState('');
  const [description, setDescription] = useState('');
  const [date, setDate] = useState(today);
  const [startTime, setStartTime] = useState('10:00');
  const [endTime, setEndTime] = useState('11:00');
  const [location, setLocation] = useState('');
  const [category, setCategory] = useState<EventCategory>('Academic');
  const [priority, setPriority] = useState<EventPriority>('Medium');
  const [flexibility, setFlexibility] = useState<EventFlexibility>('Fixed');
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    if (isEditing && selectedEvent) {
      setTitle(selectedEvent.title);
      setDescription(selectedEvent.description || '');
      setDate(selectedEvent.date || today);
      setStartTime(selectedEvent.startTime || '10:00');
      setEndTime(selectedEvent.endTime || '11:00');
      setLocation(selectedEvent.location || '');
      setCategory(selectedEvent.category || 'Academic');
      setPriority(selectedEvent.priority || 'Medium');
      setFlexibility(selectedEvent.flexibility || 'Fixed');
      setError(null);
    } else if (activeModal === 'add_event') {
      setTitle(selectedEvent?.title || '');
      setDescription(selectedEvent?.description || '');
      setDate(selectedEvent?.date || today);
      setStartTime(selectedEvent?.startTime || '10:00');
      setEndTime(selectedEvent?.endTime || '11:00');
      setLocation(selectedEvent?.location || '');
      setCategory(selectedEvent?.category || 'Meeting');
      setPriority(selectedEvent?.priority || 'Medium');
      setFlexibility(selectedEvent?.flexibility || 'Fixed');
      setError(null);
    }
  }, [activeModal, selectedEvent, isEditing, today]);

  if (!isOpen) return null;

  const handleClose = () => {
    setActiveModal(null);
    setSelectedEvent(null);
  };

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault();
    if (!title.trim()) {
      setError('Event title is required.');
      return;
    }

    const startMin = parseMinutes(startTime);
    const endMin = parseMinutes(endTime);

    if (endMin <= startMin) {
      setError('Event end time must be after start time.');
      return;
    }

    if (isEditing && selectedEvent && selectedEvent.id) {
      updateEvent({
        ...selectedEvent,
        title: title.trim(),
        description: description.trim(),
        date,
        startTime,
        endTime,
        location: location.trim(),
        category,
        priority,
        flexibility
      });
    } else {
      addEvent({
        title: title.trim(),
        description: description.trim(),
        date,
        startTime,
        endTime,
        location: location.trim(),
        category,
        priority,
        flexibility
      });
    }

    handleClose();
  };

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/40 backdrop-blur-[1px] p-4">
      <div
        className="w-full max-w-lg bg-white border border-[#DEDEDA] rounded-lg shadow-xl overflow-hidden flex flex-col max-h-[90vh]"
        onClick={e => e.stopPropagation()}
      >
        {/* Header */}
        <div className="flex items-center justify-between px-5 py-3.5 border-b border-[#DEDEDA] bg-[#FAFAF9]">
          <h3 className="text-sm font-semibold text-[#202020]">
            {isEditing ? 'Edit Calendar Event' : 'Create Calendar Event'}
          </h3>
          <button
            onClick={handleClose}
            className="text-[#777773] hover:text-[#202020] p-1 rounded hover:bg-[#E8E8E5] transition-colors"
          >
            <X className="w-4 h-4" />
          </button>
        </div>

        {/* Form Body */}
        <form onSubmit={handleSubmit} className="overflow-y-auto p-5 space-y-4 text-xs">
          {error && (
            <div className="flex items-center gap-2 p-2.5 rounded bg-[#FEF2F2] border border-[#FECACA] text-[#991B1B]">
              <AlertCircle className="w-4 h-4 flex-shrink-0" />
              <span>{error}</span>
            </div>
          )}

          {/* Title */}
          <div>
            <label className="block font-medium text-[#202020] mb-1">
              Event Title <span className="text-red-500">*</span>
            </label>
            <input
              type="text"
              required
              value={title}
              onChange={e => setTitle(e.target.value)}
              placeholder="e.g. Distributed Systems Colloquium"
              className="w-full px-3 py-2 text-xs border border-[#DEDEDA] rounded-md bg-white focus:outline-none focus:border-[#202020]"
            />
          </div>

          {/* Description */}
          <div>
            <label className="block font-medium text-[#202020] mb-1">Description</label>
            <textarea
              rows={2}
              value={description}
              onChange={e => setDescription(e.target.value)}
              placeholder="Meeting link, agenda, or prerequisites..."
              className="w-full px-3 py-2 text-xs border border-[#DEDEDA] rounded-md bg-white focus:outline-none focus:border-[#202020]"
            />
          </div>

          {/* Date & Location */}
          <div className="grid grid-cols-2 gap-4">
            <div>
              <label className="block font-medium text-[#202020] mb-1">Date</label>
              <input
                type="date"
                required
                value={date}
                onChange={e => setDate(e.target.value)}
                className="w-full px-3 py-1.5 border border-[#DEDEDA] rounded-md text-xs"
              />
            </div>
            <div>
              <label className="block font-medium text-[#202020] mb-1">Location / Room</label>
              <input
                type="text"
                value={location}
                onChange={e => setLocation(e.target.value)}
                placeholder="e.g. Lab 412 or Zoom"
                className="w-full px-3 py-1.5 border border-[#DEDEDA] rounded-md text-xs"
              />
            </div>
          </div>

          {/* Time Start & End */}
          <div className="grid grid-cols-2 gap-4">
            <div>
              <label className="block font-medium text-[#202020] mb-1">Start Time</label>
              <input
                type="time"
                required
                value={startTime}
                onChange={e => setStartTime(e.target.value)}
                className="w-full px-3 py-1.5 border border-[#DEDEDA] rounded-md text-xs"
              />
            </div>
            <div>
              <label className="block font-medium text-[#202020] mb-1">End Time</label>
              <input
                type="time"
                required
                /* a time input cannot hold "24:00"; state keeps it so an end-of-day event saves unchanged */
                value={endTime === '24:00' ? '23:59' : endTime}
                onChange={e => setEndTime(e.target.value)}
                className="w-full px-3 py-1.5 border border-[#DEDEDA] rounded-md text-xs"
              />
            </div>
          </div>

          {/* Category, Priority & Flexibility */}
          <div className="grid grid-cols-3 gap-3">
            <div>
              <label className="block font-medium text-[#202020] mb-1">Category</label>
              <select
                value={category}
                onChange={e => setCategory(e.target.value as EventCategory)}
                className="w-full px-2.5 py-1.5 border border-[#DEDEDA] rounded-md bg-white text-xs"
              >
                <option value="Academic">Academic</option>
                <option value="Meeting">Meeting</option>
                <option value="Lecture">Lecture</option>
                <option value="Work">Work</option>
                <option value="Personal">Personal</option>
                <option value="Milestone">Milestone</option>
              </select>
            </div>

            <div>
              <label className="block font-medium text-[#202020] mb-1">Priority</label>
              <select
                value={priority}
                onChange={e => setPriority(e.target.value as EventPriority)}
                className="w-full px-2.5 py-1.5 border border-[#DEDEDA] rounded-md bg-white text-xs"
              >
                <option value="High">High</option>
                <option value="Medium">Medium</option>
                <option value="Low">Low</option>
              </select>
            </div>

            <div>
              <label className="block font-medium text-[#202020] mb-1">Flexibility</label>
              <select
                value={flexibility}
                onChange={e => setFlexibility(e.target.value as EventFlexibility)}
                className="w-full px-2.5 py-1.5 border border-[#DEDEDA] rounded-md bg-white text-xs"
              >
                <option value="Fixed">Fixed (Anchor)</option>
                <option value="Flexible">Flexible (Movable)</option>
              </select>
            </div>
          </div>

          {/* Info note */}
          <div className="p-3 bg-[#F7F7F5] border border-[#DEDEDA] rounded-md text-[11px] text-[#777773] leading-relaxed">
            <span className="font-semibold text-[#202020]">Scheduling rule:</span> Fixed events act as non-negotiable anchors for the smart planner. If another event overlaps, ChronoPlan flags it for conflict resolution.
          </div>

          {/* Form Actions */}
          <div className="flex items-center justify-end gap-2 pt-3 border-t border-[#DEDEDA]">
            <button
              type="button"
              onClick={handleClose}
              className="px-3 py-1.5 text-xs text-[#555552] hover:text-[#202020] hover:bg-[#E8E8E5] rounded-md transition-colors"
            >
              Cancel
            </button>
            <button
              type="submit"
              className="px-4 py-1.5 text-xs font-medium bg-[#202020] hover:bg-[#333330] text-white rounded-md transition-colors"
            >
              {isEditing ? 'Save Changes' : 'Schedule Event'}
            </button>
          </div>
        </form>
      </div>
    </div>
  );
};
