import React, { useMemo } from 'react';
import { Link } from 'react-router-dom';
import { useApp } from '../../context/AppContext';
import { Task } from '../../types/task';
import { analyzeTaskDependencies, buildDependencyInfo, longestChains } from '../../utils/dependencyUtils';
import { computePlacements, PLACEMENT_META } from '../../utils/plannerInsights';
import { GitBranch, CheckCircle2, Ban, ArrowRight, AlertTriangle, Play, Check } from 'lucide-react';
import { PriorityBadge } from '../common/PriorityBadge';

export const DependencyGraph: React.FC = () => {
  const { tasks, toggleTaskComplete, schedulingResult, setSelectedTask, setActiveModal } = useApp();

  const analysis = useMemo(() => analyzeTaskDependencies(tasks), [tasks]);
  const info = useMemo(() => buildDependencyInfo(tasks), [tasks]);
  const chains = useMemo(() => (analysis.cycleInfo.hasCycle ? [] : longestChains(info)), [info, analysis.cycleInfo.hasCycle]);
  const placements = useMemo(() => (schedulingResult ? computePlacements(schedulingResult) : null), [schedulingResult]);

  const { completedTasks, readyTasks, blockedTasks, cycleInfo } = analysis;
  const blockedIds = useMemo(() => new Set(blockedTasks.map(t => t.id)), [blockedTasks]);
  const related = useMemo(
    () => [...info.values()].filter(n => n.prerequisites.length > 0 || n.dependents.length > 0).sort((a, b) => b.downstreamCount - a.downstreamCount || b.depth - a.depth),
    [info]
  );
  const cycleIds = useMemo(() => new Set(cycleInfo.cycleNodes), [cycleInfo.cycleNodes]);

  const openTask = (t: Task) => {
    setSelectedTask(t);
    setActiveModal('edit_task');
  };

  const stateOf = (t: Task) =>
    t.status === 'Completed' ? 'completed' : blockedIds.has(t.id) ? 'blocked' : 'ready';

  const STATE_STYLE = {
    completed: { node: 'bg-[#F0FDF4] border-[#BBF7D0]', badge: 'bg-[#DCFCE7] text-[#166534]', label: 'Completed' },
    blocked: { node: 'bg-[#FFFBEB] border-[#FDE68A]', badge: 'bg-[#FEF3C7] text-[#92400E]', label: 'Blocked' },
    ready: { node: 'bg-[#F7F7F5] border-[#202020] shadow-xs', badge: 'bg-[#202020] text-white', label: 'Ready' },
  } as const;

  const impact = (t: Task) => {
    const p = placements?.get(t.id);
    if (!schedulingResult) return <span className="text-[#777773]">No schedule generated</span>;
    if (!p) return <span className="text-[#777773]">Not in latest schedule</span>;
    return (
      <span title={p.reason ?? PLACEMENT_META[p.state].hint} className={`px-1.5 py-0.5 rounded border text-[10px] font-semibold ${PLACEMENT_META[p.state].style}`}>
        {PLACEMENT_META[p.state].label}
      </span>
    );
  };

  return (
    <div className="space-y-6">
      {cycleInfo.hasCycle && (
        <div className="p-4 bg-[#FEF2F2] border border-[#FECACA] rounded-lg text-xs text-[#991B1B]">
          <div className="flex items-center gap-2 font-semibold">
            <AlertTriangle className="w-4 h-4 flex-shrink-0" />
            <span>Circular Dependency Detected</span>
          </div>
          <p className="mt-1 text-[#7F1D1D] leading-normal">
            A closed dependency cycle prevents the engine from computing an execution order for these tasks. Remove one of the links below.
          </p>
          <div className="mt-2 p-2 bg-white/70 border border-[#FECACA] rounded font-mono text-[11px]">Cycle path: {cycleInfo.cyclePathString}</div>
        </div>
      )}

      <div className="grid grid-cols-2 md:grid-cols-4 gap-3 text-xs">
        <div className="p-3.5 bg-white border border-[#DEDEDA] rounded-md" title="Not completed, and every prerequisite is completed.">
          <div className="flex items-center justify-between text-[#777773] mb-1">
            <span className="font-medium uppercase tracking-wider text-[11px]">Ready</span>
            <Play className="w-3.5 h-3.5 text-[#202020]" />
          </div>
          <div className="text-xl font-semibold text-[#202020]">{readyTasks.length}</div>
          <p className="text-[11px] text-[#777773] mt-0.5">All prerequisites met</p>
        </div>
        <div className="p-3.5 bg-white border border-[#DEDEDA] rounded-md" title="Has at least one prerequisite that is not completed.">
          <div className="flex items-center justify-between text-[#777773] mb-1">
            <span className="font-medium uppercase tracking-wider text-[11px]">Blocked</span>
            <Ban className="w-3.5 h-3.5 text-[#92400E]" />
          </div>
          <div className="text-xl font-semibold text-[#92400E]">{blockedTasks.length}</div>
          <p className="text-[11px] text-[#777773] mt-0.5">Waiting on earlier tasks</p>
        </div>
        <div className="p-3.5 bg-white border border-[#DEDEDA] rounded-md">
          <div className="flex items-center justify-between text-[#777773] mb-1">
            <span className="font-medium uppercase tracking-wider text-[11px]">Completed</span>
            <CheckCircle2 className="w-3.5 h-3.5 text-[#166534]" />
          </div>
          <div className="text-xl font-semibold text-[#166534]">{completedTasks.length}</div>
          <p className="text-[11px] text-[#777773] mt-0.5">Prerequisites satisfied</p>
        </div>
        <div className="p-3.5 bg-white border border-[#DEDEDA] rounded-md">
          <div className="flex items-center justify-between text-[#777773] mb-1">
            <span className="font-medium uppercase tracking-wider text-[11px]">Longest chain</span>
            <GitBranch className="w-3.5 h-3.5" />
          </div>
          <div className="text-xl font-semibold text-[#202020]">{cycleInfo.hasCycle ? 'Cycle' : chains[0]?.length ?? 0}</div>
          <p className="text-[11px] text-[#777773] mt-0.5">{cycleInfo.hasCycle ? 'Fix the cycle first' : 'Tasks in the longest sequence'}</p>
        </div>
      </div>

      {/* Dependency chains from real task data */}
      <div className="bg-white border border-[#DEDEDA] rounded-lg p-5">
        <div className="pb-3 border-b border-[#DEDEDA] mb-4">
          <h3 className="text-sm font-semibold text-[#202020]">Dependency chains</h3>
          <p className="text-xs text-[#777773] mt-0.5">
            Each chain runs from a first task to the last task that depends on it. The scheduler cannot place a task before its prerequisites are done.
          </p>
        </div>

        {chains.length === 0 ? (
          <div className="text-center py-8 text-xs text-[#777773] max-w-md mx-auto leading-relaxed">
            {cycleInfo.hasCycle
              ? 'Chains are hidden while a circular dependency exists.'
              : 'No task depends on another yet. Add prerequisites when creating or editing a task to build a chain.'}
          </div>
        ) : (
          <div className="space-y-4">
            {chains.map((chain, ci) => (
              <div key={chain.map(t => t.id).join('>')} className="overflow-x-auto">
                {ci === 0 && chains.length > 1 && <div className="text-[10px] uppercase tracking-wider font-semibold text-[#777773] mb-1">Longest chain</div>}
                <div className="flex items-stretch gap-2 min-w-max pb-1">
                  {chain.map((task, index) => {
                    const st = STATE_STYLE[stateOf(task)];
                    return (
                      <React.Fragment key={task.id}>
                        <div className={`w-52 p-3 rounded-lg border ${st.node} ${cycleIds.has(task.id) ? 'ring-1 ring-[#991B1B]' : ''}`}>
                          <div className="flex items-center justify-between mb-1.5">
                            <span className="text-[10px] font-mono text-[#777773]">Step {index + 1}</span>
                            <span className={`px-1.5 rounded text-[10px] font-medium ${st.badge}`}>{st.label}</span>
                          </div>
                          <button onClick={() => openTask(task)} title="Open task" className="font-semibold text-xs text-[#202020] line-clamp-2 text-left hover:underline">
                            {task.title}
                          </button>
                          <div className="text-[11px] text-[#777773] flex items-center justify-between mt-2 pt-2 border-t border-[#DEDEDA]/60">
                            <span>{task.estimatedDuration}m</span>
                            <button
                              onClick={() => toggleTaskComplete(task.id)}
                              title={task.status === 'Completed' ? 'Mark as not completed' : 'Mark as completed'}
                              className="inline-flex items-center gap-1 px-1.5 py-0.5 rounded border border-[#DEDEDA] bg-white hover:bg-[#E8E8E5]"
                            >
                              <Check className="w-3 h-3" />
                              {task.status === 'Completed' ? 'Undo' : 'Done'}
                            </button>
                          </div>
                        </div>
                        {index < chain.length - 1 && (
                          <div className="flex items-center text-[#888884] flex-shrink-0">
                            <ArrowRight className="w-5 h-5 stroke-[1.75]" />
                          </div>
                        )}
                      </React.Fragment>
                    );
                  })}
                </div>
              </div>
            ))}
          </div>
        )}
      </div>

      {/* Per-task relations and scheduling impact */}
      <div className="bg-white border border-[#DEDEDA] rounded-lg p-4">
        <div className="flex items-center justify-between pb-2.5 border-b border-[#DEDEDA] mb-3">
          <h4 className="text-xs font-semibold text-[#202020] uppercase tracking-wider">Task relationships ({related.length})</h4>
          <Link to="/planner" className="text-[11px] underline text-[#555552] hover:text-black">Open Smart Planner</Link>
        </div>
        {related.length === 0 ? (
          <p className="text-center py-6 text-xs text-[#777773]">No dependencies defined.</p>
        ) : (
          <div className="overflow-x-auto">
            <table className="w-full text-xs min-w-[640px]">
              <thead>
                <tr className="text-left text-[11px] text-[#777773] uppercase tracking-wider">
                  <th className="py-1.5 pr-3 font-medium">Task</th>
                  <th className="py-1.5 pr-3 font-medium">State</th>
                  <th className="py-1.5 pr-3 font-medium">Needs first</th>
                  <th className="py-1.5 pr-3 font-medium">Holds up</th>
                  <th className="py-1.5 font-medium">Latest schedule</th>
                </tr>
              </thead>
              <tbody>
                {related.map(n => {
                  const st = STATE_STYLE[stateOf(n.task)];
                  return (
                    <tr key={n.task.id} className="border-t border-[#EFEFED] align-top">
                      <td className="py-2 pr-3">
                        <button onClick={() => openTask(n.task)} className="font-medium text-[#202020] hover:underline text-left">{n.task.title}</button>
                        <div className="mt-0.5"><PriorityBadge priority={n.task.priority} size="sm" /></div>
                      </td>
                      <td className="py-2 pr-3"><span className={`px-1.5 rounded text-[10px] font-medium ${st.badge}`}>{st.label}</span></td>
                      <td className="py-2 pr-3 text-[#555552]">
                        {n.prerequisites.length === 0 ? '—' : n.prerequisites.map(p => (
                          <div key={p.id} className={p.status === 'Completed' ? 'line-through text-[#999995]' : ''}>{p.title}</div>
                        ))}
                      </td>
                      <td className="py-2 pr-3 text-[#555552]">
                        {n.dependents.length === 0 ? '—' : (
                          <>
                            {n.dependents.map(d => <div key={d.id}>{d.title}</div>)}
                            {n.downstreamCount > n.dependents.length && (
                              <div className="text-[10px] text-[#777773]">+{n.downstreamCount - n.dependents.length} further downstream</div>
                            )}
                          </>
                        )}
                      </td>
                      <td className="py-2">{impact(n.task)}</td>
                    </tr>
                  );
                })}
              </tbody>
            </table>
          </div>
        )}
      </div>

      <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
        <div className="bg-white border border-[#DEDEDA] rounded-lg p-4">
          <div className="flex items-center justify-between pb-2.5 border-b border-[#DEDEDA] mb-3">
            <h4 className="text-xs font-semibold text-[#202020] uppercase tracking-wider flex items-center gap-1.5">
              <Ban className="w-3.5 h-3.5 text-[#92400E]" />
              <span>Blocked ({blockedTasks.length})</span>
            </h4>
            <span className="text-[11px] text-[#777773]">Waiting on prerequisites</span>
          </div>
          <div className="space-y-2">
            {blockedTasks.map(t => {
              const n = info.get(t.id);
              const unmet = n?.unmetPrerequisites ?? [];
              return (
                <div key={t.id} className="p-3 bg-[#FFFDF7] border border-[#FDE68A] rounded-md text-xs space-y-1">
                  <div className="flex items-center justify-between">
                    <button onClick={() => openTask(t)} className="font-semibold text-[#202020] hover:underline text-left">{t.title}</button>
                    <PriorityBadge priority={t.priority} size="sm" />
                  </div>
                  <div className="text-[11px] text-[#78350F]">
                    Blocked by: <span className="font-medium">{unmet.length ? unmet.map(u => u.title).join(', ') : t.dependencies.join(', ')}</span>
                  </div>
                  {(n?.downstreamCount ?? 0) > 0 && <div className="text-[11px] text-[#777773]">Also holds up {n!.downstreamCount} later task{n!.downstreamCount === 1 ? '' : 's'}.</div>}
                </div>
              );
            })}
            {blockedTasks.length === 0 && <div className="text-center py-6 text-xs text-[#777773]">No tasks are currently blocked.</div>}
          </div>
        </div>

        <div className="bg-white border border-[#DEDEDA] rounded-lg p-4">
          <div className="flex items-center justify-between pb-2.5 border-b border-[#DEDEDA] mb-3">
            <h4 className="text-xs font-semibold text-[#202020] uppercase tracking-wider flex items-center gap-1.5">
              <Play className="w-3.5 h-3.5 text-[#202020]" />
              <span>Ready ({readyTasks.length})</span>
            </h4>
            <span className="text-[11px] text-[#777773]">Can start now</span>
          </div>
          <div className="space-y-2">
            {readyTasks.map(t => {
              const n = info.get(t.id);
              return (
                <div key={t.id} className="p-3 bg-white border border-[#DEDEDA] rounded-md text-xs flex items-center justify-between gap-2">
                  <div className="min-w-0">
                    <button onClick={() => openTask(t)} className="font-semibold text-[#202020] hover:underline text-left truncate max-w-full">{t.title}</button>
                    <div className="text-[11px] text-[#777773] mt-0.5">
                      Est. {t.estimatedDuration}m{t.deadline ? ` • Due ${t.deadline.replace('T', ' ')}` : ''}
                      {(n?.downstreamCount ?? 0) > 0 ? ` • Unblocks ${n!.downstreamCount}` : ''}
                    </div>
                  </div>
                  <button
                    onClick={() => toggleTaskComplete(t.id)}
                    className="px-2.5 py-1 text-[11px] bg-[#F7F7F5] hover:bg-[#E8E8E5] border border-[#DEDEDA] rounded text-[#202020] transition-colors flex-shrink-0"
                  >
                    Mark Done
                  </button>
                </div>
              );
            })}
            {readyTasks.length === 0 && <div className="text-center py-6 text-xs text-[#777773]">Nothing is ready to start.</div>}
          </div>
        </div>
      </div>
    </div>
  );
};
