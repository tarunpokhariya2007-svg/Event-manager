import { Task } from '../types/task';

export interface CycleDetectionResult {
  hasCycle: boolean;
  cycleNodes: string[]; // task IDs in cycle
  cyclePathString: string;
}

export interface DependencyAnalysis {
  completedTasks: Task[];
  readyTasks: Task[];
  blockedTasks: Task[];
  cycleInfo: CycleDetectionResult;
}

/**
 * Detects circular dependencies using DFS graph traversal
 */
export function detectCircularDependencies(tasks: Task[]): CycleDetectionResult {
  const taskMap = new Map<string, Task>();
  tasks.forEach(t => taskMap.set(t.id, t));

  const visited = new Set<string>();
  const recursionStack = new Set<string>();
  const parentMap = new Map<string, string>();
  let cycleNodes: string[] = [];

  function dfs(currentId: string): boolean {
    visited.add(currentId);
    recursionStack.add(currentId);

    const task = taskMap.get(currentId);
    if (task && task.dependencies) {
      for (const depId of task.dependencies) {
        if (!taskMap.has(depId)) continue;

        if (!visited.has(depId)) {
          parentMap.set(depId, currentId);
          if (dfs(depId)) return true;
        } else if (recursionStack.has(depId)) {
          // Found cycle!
          cycleNodes = [depId, currentId];
          let curr = currentId;
          while (curr && curr !== depId && parentMap.has(curr)) {
            curr = parentMap.get(curr)!;
            cycleNodes.push(curr);
          }
          cycleNodes.reverse();
          return true;
        }
      }
    }

    recursionStack.delete(currentId);
    return false;
  }

  for (const task of tasks) {
    if (!visited.has(task.id)) {
      if (dfs(task.id)) {
        const titlePath = cycleNodes
          .map(id => taskMap.get(id)?.title || id)
          .join(' → ');
        return {
          hasCycle: true,
          cycleNodes,
          cyclePathString: titlePath
        };
      }
    }
  }

  return {
    hasCycle: false,
    cycleNodes: [],
    cyclePathString: ''
  };
}

/**
 * Analyze task dependency execution readiness
 */
export function analyzeTaskDependencies(tasks: Task[]): DependencyAnalysis {
  const completedIds = new Set(
    tasks.filter(t => t.status === 'Completed').map(t => t.id)
  );

  const completedTasks: Task[] = [];
  const readyTasks: Task[] = [];
  const blockedTasks: Task[] = [];

  for (const task of tasks) {
    if (task.status === 'Completed') {
      completedTasks.push(task);
      continue;
    }

    const hasUncompletedDeps = task.dependencies?.some(
      depId => !completedIds.has(depId)
    );

    if (hasUncompletedDeps) {
      blockedTasks.push(task);
    } else {
      readyTasks.push(task);
    }
  }

  const cycleInfo = detectCircularDependencies(tasks);

  return {
    completedTasks,
    readyTasks,
    blockedTasks,
    cycleInfo
  };
}

export interface DependencyNodeInfo {
  task: Task;
  prerequisites: Task[];     // direct prerequisites that exist in the task list
  dependents: Task[];        // tasks that list this task as a direct prerequisite
  unmetPrerequisites: Task[]; // direct prerequisites not yet completed
  downstreamCount: number;   // all tasks transitively waiting on this task
  depth: number;             // longest prerequisite path length ending at this task (0 = no prerequisites)
}

/** Builds prerequisite/dependent relations and transitive impact from the task list (cycle-safe). */
export function buildDependencyInfo(tasks: Task[]): Map<string, DependencyNodeInfo> {
  const byId = new Map(tasks.map(t => [t.id, t]));
  const dependentsOf = new Map<string, Task[]>();
  for (const t of tasks) {
    for (const d of t.dependencies ?? []) {
      if (!byId.has(d)) continue;
      const list = dependentsOf.get(d);
      if (list) list.push(t);
      else dependentsOf.set(d, [t]);
    }
  }

  const depthMemo = new Map<string, number>();
  const depthOf = (id: string, visiting: Set<string>): number => {
    const known = depthMemo.get(id);
    if (known !== undefined) return known;
    if (visiting.has(id)) return 0; // cycle guard
    visiting.add(id);
    let best = 0;
    for (const d of byId.get(id)?.dependencies ?? []) {
      if (byId.has(d)) best = Math.max(best, depthOf(d, visiting) + 1);
    }
    visiting.delete(id);
    depthMemo.set(id, best);
    return best;
  };

  const downstream = (id: string): number => {
    const seen = new Set<string>();
    const stack = [id];
    while (stack.length) {
      for (const dep of dependentsOf.get(stack.pop()!) ?? []) {
        if (!seen.has(dep.id) && dep.id !== id) {
          seen.add(dep.id);
          stack.push(dep.id);
        }
      }
    }
    return seen.size;
  };

  const info = new Map<string, DependencyNodeInfo>();
  for (const t of tasks) {
    const prerequisites = (t.dependencies ?? []).map(d => byId.get(d)).filter((x): x is Task => !!x);
    info.set(t.id, {
      task: t,
      prerequisites,
      dependents: dependentsOf.get(t.id) ?? [],
      unmetPrerequisites: prerequisites.filter(p => p.status !== 'Completed'),
      downstreamCount: downstream(t.id),
      depth: depthOf(t.id, new Set()),
    });
  }
  return info;
}

/** Longest prerequisite chains (root → ... → leaf) with at least two tasks. Returns [] when a cycle exists. */
export function longestChains(info: Map<string, DependencyNodeInfo>, limit = 5): Task[][] {
  const chains: Task[][] = [];
  for (const node of info.values()) {
    if (node.dependents.length > 0 || node.prerequisites.length === 0) continue; // chains end at leaves
    const chain: Task[] = [node.task];
    let cur = node;
    const guard = new Set([node.task.id]);
    while (cur.prerequisites.length > 0) {
      const next = cur.prerequisites
        .map(p => info.get(p.id)!)
        .sort((a, b) => b.depth - a.depth)[0];
      if (!next || guard.has(next.task.id)) break;
      guard.add(next.task.id);
      chain.unshift(next.task);
      cur = next;
    }
    if (chain.length >= 2) chains.push(chain);
  }
  return chains.sort((a, b) => b.length - a.length).slice(0, limit);
}
