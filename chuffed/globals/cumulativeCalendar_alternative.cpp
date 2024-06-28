//
// Created by Samuel Cloutier on 2023-07-19.
//

#include <chuffed/core/propagator.h>

#include <chuffed/support/reversible_sparse_set.h>
#include <chuffed/support/prefilled_sparse_set.h>
#include <chuffed/support/calendar.h>

class CumulativeCalendarProp : public Propagator {
private:
    // Resource profile of the resource
    struct ProfilePart {
        int begin;
        int end;
        int level;
        vec<int> tasks;
        ProfilePart() : begin(0), end(0), level(0), tasks() {}
    };

    // Inline functions
    struct SortLst {
        CumulativeCalendarProp *p;
        bool operator() (int i, int j) const {
            return p->actual_lst[i] < p->actual_lst[j];
        }
        explicit SortLst(CumulativeCalendarProp *_p) : p(_p) {}
    } sort_lst;

    struct SortEct {
        CumulativeCalendarProp *p;
        bool operator() (int i, int j) const {
            return p->actual_ect[i] < p->actual_ect[j];
        }
        explicit SortEct(CumulativeCalendarProp *_p) : p(_p) {}
    } sort_ect;

    int tt_profile_size;
    struct ProfilePart* tt_profile;

    ReversibleSparseSet omega;
    ReversibleSparseSet unfixed_tasks;
    PrefilledSparseSet tasks_in_profile;
    int* ordered_lst;
    int* ordered_ect;

    int* actual_lst;
    int* actual_ect;

    vec<IntVar*> start;
    vec<IntVar*> elapsed;
    vec<IntVar*> over;
    vec<int> dur;
    vec<int> usage;
    int limit;
    vec<Calendar*> cals;

public:
    CumulativeCalendarProp(vec<IntVar*>& _start, vec<IntVar*>& _over, vec<IntVar*>& _elapsed, vec<int>& _dur,
                              vec<int>& _usage, int _limit, vec<Calendar*> _cals, std::list<std::string>& opt) :
                              start(_start), over(_over), elapsed(_elapsed), dur(_dur), usage(_usage),
                              limit(_limit), sort_lst(this), sort_ect(this), omega(0, _start.size() - 1),
                              unfixed_tasks(0, _start.size() - 1), tasks_in_profile(0, _start.size() - 1),
                              cals(_cals) {

        // Allocation of the memory
        tt_profile = new ProfilePart[2 * start.size() + 1];

        // Priority of the propagator
        priority = 3;

        tt_profile_size = 0;

        ordered_lst = new int[start.size()];
        ordered_ect = new int[start.size()];

        actual_lst = new int[start.size()];
        actual_ect = new int[start.size()];

        for (int i = 0; i < start.size(); i++) {

            start[i]->attach(this, i, EVENT_LU);
            elapsed[i]->attach(this, start.size() + i, EVENT_LU);
            over[i]->attach(this, 2 * start.size() + i, EVENT_LU);
        }
    }

    // TODO Slightly change the domain used to permit better explanations
    int get_other_extremity(int task, int time, bool given_start) {
        if (given_start) {
            if (cals[task] == nullptr) return time + dur[task];
            TaskDoms task_doms(start[task]->getMin(), start[task]->getMax(), elapsed[task]->getMin(),
                               elapsed[task]->getMax(), over[task]->getMin(), over[task]->getMax());
            return cals[task]->ect(time, task_doms, dur[task]);
        }
        if (cals[task] == nullptr) return time - dur[task];
        TaskDoms task_doms(start[task]->getMin(), start[task]->getMax(), elapsed[task]->getMin(),
                           elapsed[task]->getMax(), over[task]->getMin(), over[task]->getMax());
        return cals[task]->lst(time, task_doms, dur[task]);
    }

    int actual_dur(int task, int time, bool given_start) {
        if (cals[task] == nullptr) return dur[task];
        int val = get_other_extremity(task, time, given_start);
        // Detects that no duration can be found, for a lack of solution
        if (val == INT_MIN || val == INT_MAX) {return 0;}

        if (given_start) {
            assert(val >= time);
            return val - time;
        }
        assert(val <= time);
        return time - val;
    }

    bool propagate() override {
        // It is assumed that Calendar constraints asserted bound consistency relative to the calendars for all tasks.
        // TODO Consider adding something to ascertain that the calendar bound consistency is really reached.

        for (int i = unfixed_tasks.size() - 1; i >= 0; i--) {
            // No need to verify that over is fixed since we have bound consistency with the calendar constraints
            if (start[unfixed_tasks[i]]->isFixed() && elapsed[unfixed_tasks[i]]->isFixed()) {
                unfixed_tasks.remove(unfixed_tasks[i]);
            }
        }

        int n_unfixed_tasks = unfixed_tasks.size();

        int max_profile_level = 0;
        // Computes the profile parts and checks that none overloads the resource
        if (!build_profiles(max_profile_level)) {
            return false;
        }

        int task;
        for (int i = 0; i < n_unfixed_tasks; i++) {
            task = unfixed_tasks[i];

            assert(0 <= task && task < start.size());

            if (elapsed[task]->getMin() == 0 || max_profile_level + usage[task] <= limit) {
                continue;
            }

            if (!filter_lower_bound_start_task(task)) return false;
            if (!filter_upper_bound_start_task(task)) return false;
        }

        return true;
    }

    bool build_profiles(int& max_profile_level) {
        // Tasks must first be sorted both in ascending lst, and ect.
        int n_tasks = omega.size();
        assert(n_tasks <= start.size());
        int j = 0;
        for (int i = 0; i < n_tasks; i++) {
            int actual_task_index = omega[i];
            // Computes the Calendar-correct lst and ect of every relevant task
            if (cals[actual_task_index] == nullptr) {
                actual_lst[actual_task_index] = start[actual_task_index]->getMax();
                actual_ect[actual_task_index] = start[actual_task_index]->getMin() + dur[actual_task_index];
            }
            else {
                TaskDoms task_doms(start[actual_task_index]->getMin(), start[actual_task_index]->getMax(), elapsed[actual_task_index]->getMin(),
                                   elapsed[actual_task_index]->getMax(), over[actual_task_index]->getMin(), over[actual_task_index]->getMax());
                actual_lst[actual_task_index] = cals[actual_task_index]->bound_start(task_doms, dur[actual_task_index], false);
                actual_ect[actual_task_index] = cals[actual_task_index]->ect(start[actual_task_index]->getMin(), task_doms, dur[actual_task_index]);
            }

            if (actual_lst[actual_task_index] < actual_ect[actual_task_index]) {
                assert(j < start.size());
                ordered_lst[j] = omega[i];
                ordered_ect[j] = omega[i];
                j++;
            }
        }

        n_tasks = j;
        std::sort(ordered_lst, ordered_lst + n_tasks, sort_lst);
        std::sort(ordered_ect, ordered_ect + n_tasks, sort_ect);

        int size = 1;
        int height = 0;
        int begin;
        int ending = INT_MAX;
        tasks_in_profile.clear();
        int index_lst = 0;
        int index_ect = 0;

        // Find the ending point of the new profile
        if (index_lst < n_tasks) {
            ending = actual_lst[ordered_lst[index_lst]];
        }

        tt_profile[0].begin = INT_MIN;
        tt_profile[0].end = ending;

        while (index_lst < n_tasks || index_ect < n_tasks) {
            // Find the beginning of the new profile part
            begin = ending;

            // Remove tasks that have stopped been intersected
            while (index_ect < n_tasks && actual_ect[ordered_ect[index_ect]] <= begin) {
                assert(tasks_in_profile.contains(ordered_ect[index_ect]));
                tasks_in_profile.remove(ordered_ect[index_ect]);
                height -= usage[ordered_ect[index_ect]];
                index_ect++;
            }

            // Add tasks related to this new part
            while(index_lst < n_tasks && actual_lst[ordered_lst[index_lst]] == begin) {
                assert(!tasks_in_profile.contains(ordered_lst[index_lst]));
                tasks_in_profile.insert(ordered_lst[index_lst]);
                height += usage[ordered_lst[index_lst]];
                index_lst++;
            }

            // Find the ending of the new part
            if (index_lst < n_tasks) {
                ending = actual_lst[ordered_lst[index_lst]];
            }
            if (index_ect < n_tasks && (ending > actual_ect[ordered_ect[index_ect]] || index_lst >= n_tasks)) {
                ending = actual_ect[ordered_ect[index_ect]];
            }

            if (height > limit) {
                vec<Lit> expl;
                int point_begin = begin + ((ending - begin) / 2);
                int point_end = point_begin + 1;
                analyse_tasks(expl, tasks_in_profile, height - limit - 1, point_begin, point_end);
                submit_conflict_explanation(expl);
                return false;
            }

            tt_profile[size].begin = begin;
            tt_profile[size].end = ending;
            tt_profile[size].level = height;
            tt_profile[size].tasks.clear();

            tt_profile[size].tasks.reserve(tasks_in_profile.size());
            for (int i = 0; i < tasks_in_profile.size(); i++) {
                int task = tasks_in_profile[i];
                tt_profile[size].tasks.push(task);
            }
            if (height > max_profile_level) {
                max_profile_level = height;
            }
            size++;
        }

        tt_profile[size - 1].end = INT_MAX;
        tt_profile_size = size;

        return true;
    }

    int find_profile(int t) const {
        int left = 0;
        int right = tt_profile_size - 1;
        int middle;

        while (left < right) {
            middle = (right + left + 1) / 2;
            if (tt_profile[middle].begin <= t) left = middle;
            else right = middle - 1;
        }
        assert(left == right && tt_profile[left].begin <= t && tt_profile[left].end > t);
        return left;
    }

    bool filter_lower_bound_start_task(int task) {
        int s = start[task]->getMin();
        int lst = actual_lst[task];
        int ect = actual_ect[task];

        bool has_no_comp_part = lst >= ect;

        int j = find_profile(s);
        int current_dur = actual_dur(task, s, true);
        // If we get here, a duration of zero means there is a failure
        if (current_dur == 0) {
            submit_calendar_conflict_explanation(task);
            return false;
        }

        while (j < tt_profile_size && tt_profile[j].begin < s + current_dur) {
            if (has_no_comp_part || lst >= tt_profile[j].end || ect <= tt_profile[j].begin) {
                if (limit - usage[task] < tt_profile[j].level) {
                    int expl_end = tt_profile[j].end;
                    Clause * reason = nullptr;
                    if (so.lazy) {
                        // XXX Assumption for the remaining if-statement
                        //   No compulsory part of task in profile[i]!
                        int lift_usage = tt_profile[j].level + usage[task] - limit - 1;
                        // Point-wise explanation
                        expl_end = std::min(s + current_dur, tt_profile[j].end);

                        int expl_begin = expl_end - 1;
                        vec<Lit> expl;

                        if (expl_end <= s + elapsed[task]->getMin()) {
                            // Correction by calendars was not necessary for the propagation.
                            // The same explanations as in the regular Cumulative constraint are used.

                            // Get the negated literal for [[start[task] >= expl_end - min_dur(task)]]
                            expl.push(getNegGeqLit(start[task], expl_end - elapsed[task]->getMin()));
                            // Get the negated literal for [[dur[task] >= min_dur(task)]]
                            if (elapsed[task]->getMin0() < elapsed[task]->getMin()) {
                                expl.push(getNegGeqLit(elapsed[task], elapsed[task]->getMin()));
                            }
                        }
                        else {
                            // TODO make these explanations less naive
                            // Correction by calendars was necessary for the propagation
                            if (start[task]->getMin() > start[task]->getMin0()) {
                                expl.push(start[task]->getMinLit());
                            }
                            if (elapsed[task]->getMin() > elapsed[task]->getMin0()) {
                                expl.push(elapsed[task]->getMinLit());
                            }
                            if (elapsed[task]->getMax() < elapsed[task]->getMax0()) {
                                expl.push(elapsed[task]->getMaxLit());
                            }
                            if (over[task]->getMin() > over[task]->getMin0()) {
                                expl.push(over[task]->getMinLit());
                            }
                            if (over[task]->getMax() < over[task]->getMax0()) {
                                expl.push(over[task]->getMaxLit());
                            }
                        }

                        // Get the negated literals for the tasks in the profile and the resource limit
                        analyse_tasks(expl, tt_profile[j].tasks, lift_usage, expl_begin, expl_end);

                        // Transform literals to a clause
                        reason = get_reason_for_update(expl);
                    }
//                  Impose the new lower bound on start[task]
                    if (start[task]->setMinNotR(expl_end) && !start[task]->setMin(expl_end, reason)) {
                        // Conflict occurred
                        return false;
                    }
                    s = expl_end;
                    current_dur = actual_dur(task, s, true);
                    if (current_dur == 0) {
                        submit_calendar_conflict_explanation(task);
                        return false;
                    }
                    if (expl_end < tt_profile[j].end) {
                        j--;
                    }
                }
            }
            j++;
        }

        return true;
    }

    bool filter_upper_bound_start_task(int task) {
        int lst = actual_lst[task];
        int e = get_other_extremity(task, lst, true);
        int ect = actual_ect[task];

        bool has_no_comp_part = lst >= ect;

        int j = find_profile(e - 1);
        int current_dur = actual_dur(task, e, false);
        // If we get here, a duration of zero means there is a failure
        if (current_dur == 0) {
            submit_calendar_conflict_explanation(task);
            return false;
        }
        while (j >= 0 && tt_profile[j].end > e - current_dur) {
            if (has_no_comp_part || lst >= tt_profile[j].end || ect <= tt_profile[j].begin) {
                if (limit - usage[task] < tt_profile[j].level) {
                    int expl_begin =  tt_profile[j].begin;
                    Clause * reason = nullptr;

                    // The value of the new starting time should be the lst given the lct that is expl_begin. This has to be explained by every literals.
                    int val = get_other_extremity(task, expl_begin, false);
                    if (so.lazy) {
                        // ASSUMPTION for the remaining if-statement
                        // - No compulsory part of task in profile[i]
                        int lift_usage = tt_profile[j].level + usage[task] - limit - 1;
                        // Pointwise explanation
                        expl_begin = std::max(tt_profile[j].begin, e - current_dur);

                        if (expl_begin > tt_profile[j].begin) {
                            val = get_other_extremity(task, expl_begin, false);
                        }

                        int expl_end = expl_begin + 1;
                        vec<Lit> expl;

                        // Here, "std::max(tt_profile[j].begin, start[task]->getMax())" is simply expl_begin if there was no correction by calendars.
                        if (val >= std::max(tt_profile[j].begin, start[task]->getMax()) - elapsed[task]->getMin()) {
                            // Correction by calendars was not necessary for the propagation.
                            // The same explanations as in the regular Cumulative constraint are used.

                            // Get the negated literal for [[start[task] <= expl_begin]]
                            expl.push(getNegLeqLit(start[task], std::max(tt_profile[j].begin, start[task]->getMax())));
                            // Get the negated literal for [[dur[task] >= min_dur(task)]]
                            if (elapsed[task]->getMin0() < elapsed[task]->getMin()) {
                                expl.push(getNegGeqLit(elapsed[task], elapsed[task]->getMin()));
                            }
                        }
                        else {
                            // TODO make these explanations less naive
                            // Correction by calendars was necessary for the propagation
                            if (start[task]->getMax() < start[task]->getMax0()) {
                                expl.push(start[task]->getMaxLit());
                            }
                            if (elapsed[task]->getMin() > elapsed[task]->getMin0()) {
                                expl.push(elapsed[task]->getMinLit());
                            }
                            if (elapsed[task]->getMax() < elapsed[task]->getMax0()) {
                                expl.push(elapsed[task]->getMaxLit());
                            }
                            if (over[task]->getMin() > over[task]->getMin0()) {
                                expl.push(over[task]->getMinLit());
                            }
                            if (over[task]->getMax() < over[task]->getMax0()) {
                                expl.push(over[task]->getMaxLit());
                            }
                        }

                        // Get the negated literals for the tasks in the profile and the resource limit
                        analyse_tasks(expl, tt_profile[j].tasks, lift_usage, expl_begin, expl_end);

                        // Transform literals to a clause
                        reason = get_reason_for_update(expl);
                    }


                    // Impose the new lower bound on start[task]
                    if (start[task]->setMaxNotR(val) && !start[task]->setMax(val, reason)) {
                        // Conflict occurred
                        return false;
                    }

                    e = expl_begin;
                    current_dur = actual_dur(task, e, false);
                    if (current_dur == 0) {
                        submit_calendar_conflict_explanation(task);
                        return false;
                    }
                    if (tt_profile[j].begin < expl_begin) {
                        j++;
                    }
                }
            }
            j--;
        }

        return true;
    }

    // Directly from the code of cumulative
    // Wrapper to get the negated literal -[[v <= val]] = [[v >= val + 1]]
    static inline Lit getNegLeqLit(IntVar* v, int val) {
        // return v->getLit(val + 1, LR_GE);
        return (INT_VAR_LL == v->getType() ? v->getMaxLit() : v->getLit(val + 1, LR_GE));
    }
    // Wrapper to get the negated literal -[[v >= val]] = [[ v <= val - 1]]
    static inline Lit getNegGeqLit(IntVar* v, int val) {
        // return v->getLit(val - 1, LR_LE);
        return (INT_VAR_LL == v->getType() ? v->getMinLit() : v->getLit(val - 1, LR_LE));
    }

    void analyse_tasks(vec<Lit>& expl, const PrefilledSparseSet& tasks, int slack, int point_begin, int point_end) const {
        // Point-wise explanations
        if (so.lazy) {
            for (int i = 0; i < tasks.size(); i++) {
                int task = tasks[i];
                if (slack >= usage[task]) {
                    slack -= usage[task];
                    continue;
                }

                if (start[task]->getMax() <= point_begin && point_end <= start[task]->getMin() + elapsed[task]->getMin()) {
                    // Correction by calendars was not necessary for the propagation.
                    // The same explanations as in the regular Cumulative constraint are used.

                    // Task is relevant for the resource overload
                    if (start[task]->getMin0() + elapsed[task]->getMin() <= point_end) {
                        // Lower bound of the start time variable matters
                        // Get explanation for [[start[*iter] >= end - min_dur(*iter)]]
                        expl.push(getNegGeqLit(start[task], point_end - elapsed[task]->getMin()));
                    }
                    if (point_begin < start[task]->getMax0()) {
                        // Upper bound of the start time variable matters
                        // Get explanation for [[start[*iter] <= begin]]
                        expl.push(getNegLeqLit(start[task], point_begin));
                    }
                    // Get the negated literal for [[dur[*iter] >= min_dur(*iter)]]
                    if (elapsed[task]->getMin0() < elapsed[task]->getMin()) {
                        expl.push(getNegGeqLit(elapsed[task], elapsed[task]->getMin()));
                    }
                }
                else {
                    // TODO make these explanations less naive
                    // Correction by calendars was necessary for the propagation

                    if (start[task]->getMin() > start[task]->getMin0()) {
                        expl.push(start[task]->getMinLit());
                    }
                    if (start[task]->getMax() < start[task]->getMax0()) {
                        expl.push(start[task]->getMaxLit());
                    }
                    if (elapsed[task]->getMin() > elapsed[task]->getMin0()) {
                        expl.push(elapsed[task]->getMinLit());
                    }
                    if (elapsed[task]->getMax() < elapsed[task]->getMax0()) {
                        expl.push(elapsed[task]->getMaxLit());
                    }
                    if (over[task]->getMin() > over[task]->getMin0()) {
                        expl.push(over[task]->getMinLit());
                    }
                    if (over[task]->getMax() < over[task]->getMax0()) {
                        expl.push(over[task]->getMaxLit());
                    }
                }
            }
        }
    }

    void analyse_tasks(vec<Lit>& expl, const vec<int>& tasks, int slack, int point_begin, int point_end) const {
        // Point-wise explanations
        if (so.lazy) {
            for (int i = 0; i < tasks.size(); i++) {
                int task = tasks[i];
                if (slack >= usage[task]) {
                    slack -= usage[task];
                    continue;
                }

                if (start[task]->getMax() <= point_begin && point_end <= start[task]->getMin() + elapsed[task]->getMin()) {
                    // Correction by calendars was not necessary for the propagation.
                    // The same explanations as in the regular Cumulative constraint are used.

                    // Task is relevant for the resource overload
                    if (start[task]->getMin0() + elapsed[task]->getMin() <= point_end) {
                        // Lower bound of the start time variable matters
                        // Get explanation for [[start[*iter] >= end - min_dur(*iter)]]
                        expl.push(getNegGeqLit(start[task], point_end - elapsed[task]->getMin()));
                    }
                    if (point_begin < start[task]->getMax0()) {
                        // Upper bound of the start time variable matters
                        // Get explanation for [[start[*iter] <= begin]]
                        expl.push(getNegLeqLit(start[task], point_begin));
                    }
                    // Get the negated literal for [[dur[*iter] >= min_dur(*iter)]]
                    if (elapsed[task]->getMin0() < elapsed[task]->getMin()) {
                        expl.push(getNegGeqLit(elapsed[task], elapsed[task]->getMin()));
                    }
                }
                else {
                    // TODO make these explanations less naive
                    // Correction by calendars was necessary for the propagation

                    if (start[task]->getMin() > start[task]->getMin0()) {
                        expl.push(start[task]->getMinLit());
                    }
                    if (start[task]->getMax() < start[task]->getMax0()) {
                        expl.push(start[task]->getMaxLit());
                    }
                    if (elapsed[task]->getMin() > elapsed[task]->getMin0()) {
                        expl.push(elapsed[task]->getMinLit());
                    }
                    if (elapsed[task]->getMax() < elapsed[task]->getMax0()) {
                        expl.push(elapsed[task]->getMaxLit());
                    }
                    if (over[task]->getMin() > over[task]->getMin0()) {
                        expl.push(over[task]->getMinLit());
                    }
                    if (over[task]->getMax() < over[task]->getMax0()) {
                        expl.push(over[task]->getMaxLit());
                    }
                }
            }
        }
    }

    void submit_calendar_conflict_explanation(int task) {
        // Naive explanations for a conflict due to calendars
        vec<Lit> expl;

        if (start[task]->getMin() > start[task]->getMin0()) {
            expl.push(start[task]->getMinLit());
        }
        if (start[task]->getMax() < start[task]->getMax0()) {
            expl.push(start[task]->getMaxLit());
        }
        if (elapsed[task]->getMin() > elapsed[task]->getMin0()) {
            expl.push(elapsed[task]->getMinLit());
        }
        if (elapsed[task]->getMax() < elapsed[task]->getMax0()) {
            expl.push(elapsed[task]->getMaxLit());
        }
        if (over[task]->getMin() > over[task]->getMin0()) {
            expl.push(over[task]->getMinLit());
        }
        if (over[task]->getMax() < over[task]->getMax0()) {
            expl.push(over[task]->getMaxLit());
        }

        Clause * reason = nullptr;
        if (so.lazy) {
            reason = Reason_new(expl.size());
            int i = 0;
            for (; i < expl.size(); i++) { (*reason)[i] = expl[i]; }
        }
        sat.confl = reason;
    }

    static void submit_conflict_explanation(vec<Lit> & expl) {
        Clause * reason = nullptr;
        if (so.lazy) {
            reason = Reason_new(expl.size());
            int i = 0;
            for (; i < expl.size(); i++) { (*reason)[i] = expl[i]; }
        }
        sat.confl = reason;
    }

    static Clause* get_reason_for_update(vec<Lit> & expl) {
        Clause* reason = Reason_new(expl.size() + 1);
        for (int i = 1; i <= expl.size(); i++) {
            (*reason)[i] = expl[i-1];
        }
        return reason;
    }
};

void cumulative_calendar_day(vec<IntVar*>& s, vec<IntVar*>& o, vec<IntVar*>& e, vec<int>& d, vec<int>& r, int limit, const vec<std::vector<int>>& calendars, const vec<int>& cals_followed, std::list<std::string>& opt) {
    // ASSUMPTION
    // - s, o, e, d, r, and cals_followed contain the same number of elements
    rassert(s.size() == e.size());
    rassert(s.size() == o.size());
    rassert(s.size() == d.size());
    rassert(s.size() == r.size());
    rassert(s.size() == cals_followed.size());
    for (int i = 0; i < s.size(); i++) {
        rassert(o[i]->getMin() >= 0);
        rassert(d[i] >= 0);
        rassert(r[i] >= 0);
        rassert(-1 <= cals_followed[i] && cals_followed[i] < calendars.size());
        rassert(e[i]->getMin() >= 0);
    }
    for (int i = 1; i < calendars.size(); i++) {
        rassert(calendars[i - 1].size() == calendars[i].size());
    }
    for (int i = 0; i < calendars.size(); i++) {
        for (int j : calendars[i]) {
            rassert(0 <= j && j <= 1);
        }
    }

    vec<Calendar*> cals;
    cals.reserve(s.size());
    for (int i = 0; i < s.size(); i++) {
        if (cals_followed[i] != -1) {
//            if (o[i]->getMax() > 0) {
                cals.push(&CalendarFactory::get_calendar_day(calendars[cals_followed[i]]));
//            }
//            else {
//                cals.push(&CalendarFactory::get_calendar_no_over(calendars[cals_followed[i]]));
//            }
        }
        else {
            cals.push(nullptr);
        }
    }

    new CumulativeCalendarProp(s, o, e, d, r, limit, cals, opt);
}

void cumulative_calendar_hour(vec<IntVar*>& s, vec<IntVar*>& o, vec<IntVar*>& e, vec<int>& d, vec<int>& r, int limit, const vec<std::vector<int>>& calendars, const vec<int>& cals_followed, std::list<std::string>& opt) {
    // ASSUMPTION
    // - s, o, e, d, r, and cals_followed contain the same number of elements
    rassert(s.size() == e.size());
    rassert(s.size() == o.size());
    rassert(s.size() == d.size());
    rassert(s.size() == r.size());
    rassert(s.size() == cals_followed.size());
    for (int i = 0; i < s.size(); i++) {
        rassert(o[i]->getMin() >= 0);
        rassert(d[i] >= 0);
        rassert(r[i] >= 0);
        rassert(-1 <= cals_followed[i] && cals_followed[i] < calendars.size());
        rassert(e[i]->getMin() >= 0);
    }
    for (int i = 1; i < calendars.size(); i++) {
        rassert(calendars[i - 1].size() == calendars[i].size());
    }

//    std::vector<bool> available_over(s.size(), false);

    for (int i = 0; i < calendars.size(); i++) {
        for (int j : calendars[i]) {
            rassert(0 <= j && j <= 2);
//            if (j == 2 && !available_over[i]) {
//                available_over[i] = true;
//            }
        }
    }

    vec<Calendar*> cals;
    cals.reserve(s.size());
    for (int i = 0; i < s.size(); i++) {
        if (cals_followed[i] != -1) {
//            if (o[i]->getMax() > 0 && available_over[cals_followed[i]]) {
                cals.push(&CalendarFactory::get_calendar_hour(calendars[cals_followed[i]]));
//            }
//            else {
//                if (o[i]->getMax() > 0) {
//                    int_rel(o[i], IRT_EQ, 0);
//                }
//                cals.push(&CalendarFactory::get_calendar_no_over(calendars[cals_followed[i]]));
//            }
        }
        else {
            cals.push(nullptr);
        }
    }

    new CumulativeCalendarProp(s, o, e, d, r, limit, cals, opt);
}
