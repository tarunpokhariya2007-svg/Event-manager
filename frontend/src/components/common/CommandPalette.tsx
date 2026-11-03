import React, { useState, useEffect, useRef } from 'react';
import { useNavigate } from 'react-router-dom';
import { useApp } from '../../context/AppContext';
import { Search, Calendar, CheckSquare, Sparkles, AlertCircle, ArrowRight, X, Clock } from 'lucide-react';
import { formatTime12h, formatDateReadable } from '../../utils/timeUtils';

export const CommandPalette: React.FC = () => {
  const { activeModal, setActiveModal, tasks, events, setSelectedTask, setSelectedEvent } = useApp();
  const [query, setQuery] = useState('');
  const [selectedIndex, setSelectedIndex] = useState(0);
  const inputRef = useRef<HTMLInputElement>(null);
  const navigate = useNavigate();

  const isOpen = activeModal === 'command_palette';

  // Global keydown listener for Cmd/Ctrl+K
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      if ((e.metaKey || e.ctrlKey) && e.key === 'k') {
        e.preventDefault();
        setActiveModal(isOpen ? null : 'command_palette');
      } else if (e.key === 'Escape' && isOpen) {
        setActiveModal(null);
      }
    };
    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [isOpen, setActiveModal]);

  useEffect(() => {
    if (isOpen) {
      setQuery('');
      setSelectedIndex(0);
      setTimeout(() => inputRef.current?.focus(), 50);
    }
  }, [isOpen]);

  if (!isOpen) return null;

  const filteredTasks = tasks.filter(t =>
    t.title.toLowerCase().includes(query.toLowerCase()) ||
    t.category?.toLowerCase().includes(query.toLowerCase())
  ).slice(0, 4);

  const filteredEvents = events.filter(e =>
    e.title.toLowerCase().includes(query.toLowerCase()) ||
    e.location?.toLowerCase().includes(query.toLowerCase())
  ).slice(0, 4);

  const navItems = [
    { title: 'Overview', path: '/dashboard', icon: ArrowRight },
    { title: 'My Schedule', path: '/schedule', icon: Calendar },
    { title: 'Tasks Management', path: '/tasks', icon: CheckSquare },
    { title: 'Events Management', path: '/events', icon: Calendar },
    { title: 'Smart Planner Engine', path: '/planner', icon: Sparkles },
    { title: 'Conflict Resolution', path: '/conflicts', icon: AlertCircle },
    { title: 'Task Dependencies Graph', path: '/dependencies', icon: ArrowRight },
    { title: 'Productivity Analytics', path: '/analytics', icon: ArrowRight },
    { title: 'System Settings', path: '/settings', icon: ArrowRight }
  ].filter(n => n.title.toLowerCase().includes(query.toLowerCase()));

  return (
    <div className="fixed inset-0 z-50 flex items-start justify-center pt-20 bg-black/40 backdrop-blur-[1px] p-4">
      <div
        className="w-full max-w-xl bg-white border border-[#DEDEDA] rounded-lg shadow-xl overflow-hidden flex flex-col max-h-[75vh]"
        onClick={e => e.stopPropagation()}
      >
        {/* Search Input Bar */}
        <div className="flex items-center px-3.5 py-3 border-b border-[#DEDEDA] gap-2.5">
          <Search className="w-4 h-4 text-[#777773]" />
          <input
            ref={inputRef}
            type="text"
            value={query}
            onChange={e => setQuery(e.target.value)}
            placeholder="Search tasks, events, schedules, or jump to page... (Esc to close)"
            className="w-full text-sm text-[#202020] placeholder-[#888884] bg-transparent focus:outline-none"
          />
          {query && (
            <button onClick={() => setQuery('')} className="text-[#888884] hover:text-[#202020]">
              <X className="w-3.5 h-3.5" />
            </button>
          )}
          <span className="text-[10px] bg-[#EFEFED] text-[#666662] px-1.5 py-0.5 rounded border border-[#DEDEDA]">
            ESC
          </span>
        </div>

        {/* Results List */}
        <div className="overflow-y-auto p-2 space-y-4 text-xs">
          {/* Quick Navigation */}
          {navItems.length > 0 && (
            <div>
              <div className="px-2 py-1 text-[11px] font-medium text-[#777773] uppercase tracking-wider">
                Pages &amp; Views
              </div>
              <div className="space-y-0.5 mt-1">
                {navItems.map(item => (
                  <button
                    key={item.path}
                    onClick={() => {
                      navigate(item.path);
                      setActiveModal(null);
                    }}
                    className="w-full flex items-center justify-between px-2.5 py-2 rounded text-left text-[#202020] hover:bg-[#E8E8E5] transition-colors"
                  >
                    <div className="flex items-center gap-2">
                      <item.icon className="w-3.5 h-3.5 text-[#777773]" />
                      <span className="font-medium">{item.title}</span>
                    </div>
                    <span className="text-[11px] text-[#777773]">{item.path}</span>
                  </button>
                ))}
              </div>
            </div>
          )}

          {/* Tasks */}
          {filteredTasks.length > 0 && (
            <div>
              <div className="px-2 py-1 text-[11px] font-medium text-[#777773] uppercase tracking-wider">
                Tasks
              </div>
              <div className="space-y-0.5 mt-1">
                {filteredTasks.map(task => (
                  <button
                    key={task.id}
                    onClick={() => {
                      setSelectedTask(task);
                      setActiveModal('edit_task');
                    }}
                    className="w-full flex items-center justify-between px-2.5 py-2 rounded text-left text-[#202020] hover:bg-[#E8E8E5] transition-colors"
                  >
                    <div className="flex items-center gap-2 truncate">
                      <CheckSquare className="w-3.5 h-3.5 text-[#777773] flex-shrink-0" />
                      <span className="truncate">{task.title}</span>
                    </div>
                    <span className="text-[11px] text-[#777773] flex-shrink-0 ml-2">
                      {task.priority} Priority
                    </span>
                  </button>
                ))}
              </div>
            </div>
          )}

          {/* Events */}
          {filteredEvents.length > 0 && (
            <div>
              <div className="px-2 py-1 text-[11px] font-medium text-[#777773] uppercase tracking-wider">
                Events &amp; Appointments
              </div>
              <div className="space-y-0.5 mt-1">
                {filteredEvents.map(evt => (
                  <button
                    key={evt.id}
                    onClick={() => {
                      setSelectedEvent(evt);
                      setActiveModal('event_details');
                    }}
                    className="w-full flex items-center justify-between px-2.5 py-2 rounded text-left text-[#202020] hover:bg-[#E8E8E5] transition-colors"
                  >
                    <div className="flex items-center gap-2 truncate">
                      <Clock className="w-3.5 h-3.5 text-[#777773] flex-shrink-0" />
                      <span className="truncate">{evt.title}</span>
                    </div>
                    <span className="text-[11px] text-[#777773] flex-shrink-0 ml-2">
                      {formatTime12h(evt.startTime)} ({formatDateReadable(evt.date)})
                    </span>
                  </button>
                ))}
              </div>
            </div>
          )}

          {navItems.length === 0 && filteredTasks.length === 0 && filteredEvents.length === 0 && (
            <div className="py-8 text-center text-[#777773]">
              No results found for &ldquo;{query}&rdquo;
            </div>
          )}
        </div>

        {/* Footer */}
        <div className="px-3.5 py-2 bg-[#FAFAF9] border-t border-[#DEDEDA] flex items-center justify-between text-[11px] text-[#777773]">
          <span>Tip: Press ESC to close</span>
          <span>ChronoPlan v1.0.4 • Monochromatic UI</span>
        </div>
      </div>
    </div>
  );
};
