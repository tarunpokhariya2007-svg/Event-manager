import React, { useState, useMemo } from 'react';
import { useApp } from '../../context/AppContext';
import { CalendarEvent, EventCategory, EventPriority, EventSortField } from '../../types/event';
import {
  Search,
  Plus,
  Trash2,
  Edit2,
  Calendar,
  Clock,
  MapPin,
  AlertTriangle,
  ArrowUpDown
} from 'lucide-react';
import { PriorityBadge } from '../common/PriorityBadge';
import { formatTime12h, formatDateReadable, parseMinutes } from '../../utils/timeUtils';
import { ConfirmDialog } from '../common/ConfirmDialog';
import { EmptyState } from '../common/EmptyState';

export const EventTable: React.FC = () => {
  const {
    events,
    conflicts,
    deleteEvent,
    setActiveModal,
    setSelectedEvent
  } = useApp();

  const [search, setSearch] = useState('');
  const [categoryFilter, setCategoryFilter] = useState<'All' | EventCategory>('All');
  const [sortOrder, setSortOrder] = useState<'asc' | 'desc'>('asc');
  const [eventToDelete, setEventToDelete] = useState<CalendarEvent | null>(null);

  // Check if an event is in a conflict
  const isEventConflicted = (eventId: string) => {
    return conflicts.some(c => c.eventA.id === eventId || c.eventB.id === eventId);
  };

  const filteredAndSortedEvents = useMemo(() => {
    return events
      .filter(evt => {
        const matchesSearch =
          evt.title.toLowerCase().includes(search.toLowerCase()) ||
          evt.location?.toLowerCase().includes(search.toLowerCase()) ||
          evt.description?.toLowerCase().includes(search.toLowerCase());

        const matchesCategory =
          categoryFilter === 'All' || evt.category === categoryFilter;

        return matchesSearch && matchesCategory;
      })
      .sort((a, b) => {
        const dateA = new Date(`${a.date}T${a.startTime}`).getTime();
        const dateB = new Date(`${b.date}T${b.startTime}`).getTime();
        return sortOrder === 'asc' ? dateA - dateB : dateB - dateA;
      });
  }, [events, search, categoryFilter, sortOrder]);

  const handleEdit = (evt: CalendarEvent) => {
    setSelectedEvent(evt);
    setActiveModal('edit_event');
  };

  const confirmDelete = () => {
    if (eventToDelete) {
      deleteEvent(eventToDelete.id);
      setEventToDelete(null);
    }
  };

  return (
    <div className="space-y-4">
      {/* Table Toolbar */}
      <div className="bg-white border border-[#DEDEDA] rounded-lg shadow-xs overflow-hidden">
        <div className="p-3.5 border-b border-[#DEDEDA] flex flex-col md:flex-row md:items-center justify-between gap-3 bg-[#FAFAF9]">
          {/* Search */}
          <div className="relative flex-1 max-w-sm">
            <Search className="w-3.5 h-3.5 text-[#777773] absolute left-3 top-2.5" />
            <input
              type="text"
              value={search}
              onChange={e => setSearch(e.target.value)}
              placeholder="Filter events by title, room, notes..."
              className="w-full pl-8 pr-3 py-1.5 text-xs border border-[#DEDEDA] rounded-md bg-white focus:outline-none focus:border-[#202020]"
            />
          </div>

          {/* Filters & Actions */}
          <div className="flex items-center gap-2 flex-wrap">
            <select
              value={categoryFilter}
              onChange={e => setCategoryFilter(e.target.value as any)}
              className="px-2.5 py-1.5 text-xs border border-[#DEDEDA] rounded-md bg-white text-[#202020]"
            >
              <option value="All">All Categories</option>
              <option value="Academic">Academic</option>
              <option value="Meeting">Meeting</option>
              <option value="Lecture">Lecture</option>
              <option value="Work">Work</option>
              <option value="Personal">Personal</option>
              <option value="Milestone">Milestone</option>
            </select>

            <button
              onClick={() => setSortOrder(prev => (prev === 'asc' ? 'desc' : 'asc'))}
              className="px-2.5 py-1.5 text-xs border border-[#DEDEDA] rounded-md bg-white text-[#202020] hover:bg-[#E8E8E5] transition-colors flex items-center gap-1.5"
            >
              <ArrowUpDown className="w-3 h-3 text-[#777773]" />
              <span>{sortOrder === 'asc' ? 'Chronological' : 'Reverse'}</span>
            </button>

            <button
              onClick={() => {
                setSelectedEvent(null);
                setActiveModal('add_event');
              }}
              className="inline-flex items-center gap-1.5 px-3 py-1.5 bg-[#202020] hover:bg-[#333330] text-white text-xs font-medium rounded-md transition-colors"
            >
              <Plus className="w-3.5 h-3.5" />
              <span>Add Event</span>
            </button>
          </div>
        </div>

        {/* Overlap Summary Notice */}
        {conflicts.length > 0 && (
          <div className="px-4 py-2 bg-[#FFFBEB] border-b border-[#FDE68A] text-xs text-[#92400E] flex items-center justify-between">
            <div className="flex items-center gap-2">
              <AlertTriangle className="w-3.5 h-3.5" />
              <span>
                Detected {conflicts.length} overlapping schedule conflict(s). Highlighted below.
              </span>
            </div>
            <a href="/conflicts" className="underline font-semibold hover:text-[#78350F]">
              Resolve Conflict →
            </a>
          </div>
        )}

        {/* Table */}
        {filteredAndSortedEvents.length === 0 ? (
          <div className="p-8">
            <EmptyState
              title="No events found"
              description="No calendar events match the current filter."
              actionText="Add New Event"
              onAction={() => {
                setSearch('');
                setCategoryFilter('All');
                setActiveModal('add_event');
              }}
            />
          </div>
        ) : (
          <div className="overflow-x-auto">
            <table className="w-full text-left text-xs">
              <thead className="bg-[#FAFAF9] border-b border-[#DEDEDA] text-[11px] text-[#777773] uppercase tracking-wider select-none font-medium">
                <tr>
                  <th className="py-2.5 px-4">Event Name</th>
                  <th className="py-2.5 px-4">Date</th>
                  <th className="py-2.5 px-4">Time</th>
                  <th className="py-2.5 px-4">Category</th>
                  <th className="py-2.5 px-4">Priority</th>
                  <th className="py-2.5 px-4">Flexibility</th>
                  <th className="py-2.5 px-4">Location</th>
                  <th className="py-2.5 px-4 text-right">Actions</th>
                </tr>
              </thead>
              <tbody className="divide-y divide-[#EFEFED]">
                {filteredAndSortedEvents.map(evt => {
                  const conflicted = isEventConflicted(evt.id);

                  return (
                    <tr
                      key={evt.id}
                      className={`hover:bg-[#F7F7F5] transition-colors group ${
                        conflicted ? 'bg-[#FFFDF7]' : ''
                      }`}
                    >
                      {/* Name */}
                      <td className="py-3 px-4 max-w-xs">
                        <div className="flex items-center gap-1.5 font-medium text-[#202020]">
                          <span>{evt.title}</span>
                          {conflicted && (
                            <span
                              title="Time slot overlaps with another event"
                              className="px-1.5 py-0.2 text-[10px] rounded bg-[#FFFBEB] text-[#92400E] border border-[#FDE68A] font-medium"
                            >
                              Overlap
                            </span>
                          )}
                        </div>
                        {evt.description && (
                          <div className="text-[11px] text-[#777773] truncate mt-0.5">
                            {evt.description}
                          </div>
                        )}
                      </td>

                      {/* Date */}
                      <td className="py-3 px-4 whitespace-nowrap text-[#555552]">
                        {formatDateReadable(evt.date)}
                      </td>

                      {/* Time */}
                      <td className="py-3 px-4 whitespace-nowrap font-mono text-[#202020]">
                        {formatTime12h(evt.startTime)} – {formatTime12h(evt.endTime)}
                      </td>

                      {/* Category */}
                      <td className="py-3 px-4 whitespace-nowrap">
                        <span className="px-2 py-0.5 rounded bg-[#EFEFED] text-[#555552] border border-[#DEDEDA] text-[11px]">
                          {evt.category}
                        </span>
                      </td>

                      {/* Priority */}
                      <td className="py-3 px-4 whitespace-nowrap">
                        <PriorityBadge priority={evt.priority} size="sm" />
                      </td>

                      {/* Flexibility */}
                      <td className="py-3 px-4 whitespace-nowrap">
                        <span
                          className={`text-[11px] ${
                            evt.flexibility === 'Fixed'
                              ? 'font-medium text-[#202020]'
                              : 'text-[#777773]'
                          }`}
                        >
                          {evt.flexibility}
                        </span>
                      </td>

                      {/* Location */}
                      <td className="py-3 px-4 whitespace-nowrap text-[#777773]">
                        {evt.location || '—'}
                      </td>

                      {/* Actions */}
                      <td className="py-3 px-4 text-right whitespace-nowrap">
                        <div className="flex items-center justify-end gap-1 opacity-80 group-hover:opacity-100 transition-opacity">
                          <button
                            onClick={() => handleEdit(evt)}
                            className="p-1 text-[#777773] hover:text-[#202020] hover:bg-[#E8E8E5] rounded transition-colors"
                            title="Edit event"
                          >
                            <Edit2 className="w-3.5 h-3.5" />
                          </button>
                          <button
                            onClick={() => setEventToDelete(evt)}
                            className="p-1 text-[#777773] hover:text-[#991B1B] hover:bg-[#FEF2F2] rounded transition-colors"
                            title="Delete event"
                          >
                            <Trash2 className="w-3.5 h-3.5" />
                          </button>
                        </div>
                      </td>
                    </tr>
                  );
                })}
              </tbody>
            </table>
          </div>
        )}
      </div>

      {/* Delete Confirmation Modal */}
      <ConfirmDialog
        isOpen={Boolean(eventToDelete)}
        title="Delete Event"
        message={`Are you sure you want to delete "${eventToDelete?.title}"? This event will be removed from your calendar.`}
        confirmText="Delete Event"
        onConfirm={confirmDelete}
        onCancel={() => setEventToDelete(null)}
      />
    </div>
  );
};
