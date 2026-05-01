include "chuffed.mzn";
include "fzn_cumulative_kaoc.mzn";
include "fzn_cumulative.mzn";

int: horizon;
set of int: time_points = 0..horizon;

int: n_tasks;
set of int: tasks = 1..n_tasks;
array[tasks] of int: processing_times;

int: n_resources;
set of int: resources = 1..n_resources;
array[resources] of int: resource_capacities;

int: n_resource_usages;
set of int: consomptions = 1..n_resource_usages;
array[consomptions] of tasks: resource_usage_tasks;
array[consomptions] of int: resource_usage_quantity;
array[consomptions] of resources: resource_usage_resources;
array[resources] of set of consomptions: consomptions_per_resource = [{a | a in consomptions where resource_usage_resources[a] == r} | r in resources];
array[tasks] of set of consomptions: consomptions_per_task = [{a | a in consomptions where resource_usage_tasks[a] == i} | i in tasks];

int: n_precedences;
set of int: precedences = 1..n_precedences;
array[precedences] of tasks: task_before;
array[precedences] of tasks: task_after;

array[tasks] of var time_points: start_times;

var time_points: makespan;

% Basic constraints

constraint forall(i in tasks)(start_times[i] + processing_times[i] - 1 <= horizon);

% Takes care of the precedences
constraint forall(i in precedences)(start_times[task_before[i]] + processing_times[task_before[i]] <= start_times[task_after[i]]);

% Cumulative constraint
constraint forall(r in resources where consomptions_per_resource[r] != {})(
  fzn_cumulative_kaoc([start_times[resource_usage_tasks[a]] | a in consomptions_per_resource[r]],
             [processing_times[resource_usage_tasks[a]] | a in consomptions_per_resource[r]],
             [resource_usage_quantity[a] | a in consomptions_per_resource[r]], resource_capacities[r]) :: holes(false) :: kaoc_lifting(true)
);
constraint forall(r in resources where consomptions_per_resource[r] != {})(
  fzn_cumulative([start_times[resource_usage_tasks[a]] | a in consomptions_per_resource[r]],
             [processing_times[resource_usage_tasks[a]] | a in consomptions_per_resource[r]],
             [resource_usage_quantity[a] | a in consomptions_per_resource[r]], resource_capacities[r]) :: ttef_check(false) :: ttef_filt(false)
);

constraint makespan = max([start_times[i] + processing_times[i]| i in tasks]);

ann: search_strategy = int_search(start_times, smallest, indomain_min, complete);

ann: restart_strategy = restart_constant(100);

solve :: search_strategy :: restart_strategy minimize makespan;

output [show(start_times[i]) ++ if i < n_tasks then ", " else "\n" endif | i in tasks];
