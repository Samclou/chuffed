#include <chuffed/core/propagator.h>
#include <chuffed/support/bit_set.h>
#include <chuffed/support/prefilled_sparse_set.h>

#include <algorithm>

// #define VERBOSE_PROPAGATION_VERIFICATION

#ifdef VERBOSE_PROPAGATION_VERIFICATION
#include <iostream>
#define assert(e) rassert(e)
#endif


template <bool is_knapsack = true>
class CumulativeKAOC: public Propagator {
private:
    typedef uint64_t uint_bitset;

    int n_tasks;

    vec<IntVar*> start;
    vec<IntVar*> dur;
    vec<IntVar*> usage;
    vec<int> e;
    int limit;

    int* est;
    int* ordered_est;
    int* ect;
    int* ordered_ect;
    int* lst;
    int* ordered_lst;
    int* lct;
    int* ordered_lct;

    PrefilledSparseSet tasks_in_profile;
    PrefilledSparseSet tasks_in_lambda;
    PrefilledSparseSet tasks_in_omega;

#ifdef VERBOSE_PROPAGATION_VERIFICATION
    bool present;
#endif

    struct TimePoint {
        int time;
        int next_point_position;
        int previous_point_position;
        int over;
        int req;
        BitSet<uint_bitset, is_knapsack> knapsack;

        explicit TimePoint(int limit) : knapsack(limit + 1, limit) {
            time = 0;
            next_point_position = 0;
            previous_point_position = 0;
            over = 0;
            req = 0;
        }
    };

    std::vector<TimePoint> time_points;
    int* pos_est;
    int first_relevant_tp;
    int first_added_tp;
    int first_unused_added_tp;

    bool kaoc_expl_lifting;

public:
    CumulativeKAOC(vec<IntVar*>& _start, vec<IntVar*>& _dur, vec<IntVar*>& _usage, int _limit,
                   std::list<std::string>& opt) : start(_start), dur(_dur), usage(_usage),
                                                  limit(_limit), tasks_in_profile(0, _start.size()), tasks_in_lambda(0, _start.size()), tasks_in_omega(0, _start.size()) {

        priority = 3;

        n_tasks = start.size();

        for (int i = 0; i < n_tasks; i++) {
            start[i]->attach(this, i, EVENT_LU);
            dur[i]->attach(this, n_tasks + i, EVENT_L);
            usage[i]->attach(this, (2 * n_tasks) + i, EVENT_L);
        }

        e.reserve(n_tasks);
        est = new int[n_tasks];
        ordered_est = new int[n_tasks];
        ect = new int[n_tasks];
        ordered_ect = new int[n_tasks];
        lst = new int[n_tasks];
        ordered_lst = new int[n_tasks];
        lct = new int[n_tasks];
        ordered_lct = new int[n_tasks];
        for (int i = 0; i < n_tasks; i++) {
            e.push(dur[i]->getMin() * usage[i]->getMin());
            ordered_est[i] = i;
            ordered_ect[i] = i;
            ordered_lst[i] = i;
            ordered_lct[i] = i;
        }

        kaoc_expl_lifting = true;
        // Overriding option defaults
        for (auto& it : opt) {
            if (it == "kaoc_lifting_on") {
                kaoc_expl_lifting = true;
            } else if (it == "kaoc_lifting_off") {
                kaoc_expl_lifting = false;
            }
        }

        time_points.reserve((6 * n_tasks) + 1 + 2);
        for (int i = 0; i < (6 * n_tasks) + 1 + 2; i++) {
            time_points.emplace_back(limit);
        }
        pos_est = new int[n_tasks];
        first_relevant_tp = INT_MAX;
        first_added_tp = 0;
        first_unused_added_tp = 0;

#ifdef VERBOSE_PROPAGATION_VERIFICATION
        present = false;
        present_propagator();
#endif
    }

    ~CumulativeKAOC() override {
        delete[] est;
        delete[] lst;
        delete[] ect;
        delete[] lct;
        delete[] ordered_est;
        delete[] ordered_lst;
        delete[] ordered_ect;
        delete[] ordered_lct;

        delete[] pos_est;
    }

    bool propagate() override {
#ifdef VERBOSE_PROPAGATION_VERIFICATION
        present = false;
#endif
        for (int i = 0; i < n_tasks; i++) {
            est[i] = start[i]->getMin();
            ect[i] = est[i] + dur[i]->getMin();
            lst[i] = start[i]->getMax();
            lct[i] = lst[i] + dur[i]->getMin();
            e[i] = dur[i]->getMin() * usage[i]->getMin();
        }

        sort_by_proxy(ordered_est, est, n_tasks);
        sort_by_proxy(ordered_ect, ect, n_tasks);
        sort_by_proxy(ordered_lst, lst, n_tasks);
        sort_by_proxy(ordered_lct, lct, n_tasks);
        assert(verif_order());

        if (!knapsack_augmented_overload_check()) {
            return false;
        }
        return true;
    }

    // Computes all time-points and computes the profile height at each one, performing tt-checking. When full_set is
    // true or unspecified, tasks_in_profile and tasks_in_omega are filled with all relevant tasks. Otherwise, only
    // tasks in one of those sets are taken into account.
    // Complexity: O(Cn)
    bool init_structure(bool full_set=true) {
        assert(verif_order());

        if (full_set) {
            tasks_in_omega.fill();
            tasks_in_profile.clear();
        }

        int i = 0;
        int i_est = 0;
        int i_ect = 0;
        int i_lst = 0;
        int i_lct = 0;

        int current_time = est[ordered_est[i_est]];
        int next_time;
        int profile_height = 0;
        while (i_lct < n_tasks) {
            // Cleaning up repeated values and dealing with time-specific tasks
            while (i_est < n_tasks && (est[ordered_est[i_est]] == current_time || !(tasks_in_omega.contains(ordered_est[i_est]) || tasks_in_profile.contains(ordered_est[i_est])))) {
                pos_est[ordered_est[i_est]] = i;
                i_est++;
            }
            while (i_ect < n_tasks && (ect[ordered_ect[i_ect]] == current_time || !(tasks_in_omega.contains(ordered_ect[i_ect]) || tasks_in_profile.contains(ordered_ect[i_ect])))) {
                if (lst[ordered_ect[i_ect]] < ect[ordered_ect[i_ect]] && tasks_in_profile.contains(ordered_ect[i_ect])) {
                    profile_height -= usage[ordered_ect[i_ect]]->getMin();
                }
                i_ect++;
            }
            while (i_lst < n_tasks && (lst[ordered_lst[i_lst]] == current_time || !(tasks_in_omega.contains(ordered_lst[i_lst]) || tasks_in_profile.contains(ordered_lst[i_lst])))) {
                if (lst[ordered_lst[i_lst]] < ect[ordered_lst[i_lst]] && (full_set || tasks_in_profile.contains(ordered_lst[i_lst]))) {
                    profile_height += usage[ordered_lst[i_lst]]->getMin();
                    if (full_set) {
                        tasks_in_profile.insert(ordered_lst[i_lst]);
                    }
                }
                i_lst++;
            }
            while (i_lct < n_tasks && (lct[ordered_lct[i_lct]] == current_time || !(tasks_in_omega.contains(ordered_lct[i_lct]) || tasks_in_profile.contains(ordered_lct[i_lct])))) {
                i_lct++;
            }

            // Finding the next time value
            next_time = INT_MAX;
            if (i_est < n_tasks && next_time > est[ordered_est[i_est]]) {
                next_time = est[ordered_est[i_est]];
            }
            if (i_ect < n_tasks && next_time > ect[ordered_ect[i_ect]]) {
                next_time = ect[ordered_ect[i_ect]];
            }
            if (i_lst < n_tasks && next_time > lst[ordered_lst[i_lst]]) {
                next_time = lst[ordered_lst[i_lst]];
            }
            if (i_lct < n_tasks && next_time > lct[ordered_lct[i_lct]]) {
                next_time = lct[ordered_lct[i_lct]];
            }

            if (profile_height > limit) {
                assert(i_lct < n_tasks);
                vec<Lit> expl;
                int point_begin = current_time + ((next_time - current_time) / 2);
                int point_end = point_begin + 1;
                int slack = profile_height - limit - 1;
#ifdef VERBOSE_PROPAGATION_VERIFICATION
                report_propagation("Conflict (Time-Tabling, kaoc)");
#endif
                if (so.lazy) {
                    for (int j = 0; j < n_tasks; j++) {
                        if (lst[j] > point_begin || point_begin >= ect[j]) {
                            continue;
                        }
                        if (slack >= usage[j]->getMin()) {
                            slack -= usage[j]->getMin();
                            continue;
                        }
                        // Task is relevant for the resource overload
                        if (start[j]->getMin0() + dur[j]->getMin() <= point_end) {
                            // Lower bound of the start time variable matters
                            // Get explanation for [[start[*iter] >= end - min_dur(*iter)]]
                            expl.push(getNegGeqLit(start[j], point_end - dur[j]->getMin()));
#ifdef VERBOSE_PROPAGATION_VERIFICATION
                            std::cout << "LB_start, " << j << ", " << point_end - dur[j]->getMin() << "; ";
#endif
                        }
                        if (point_begin < start[j]->getMax0()) {
                            // Upper bound of the start time variable matters
                            // Get explanation for [[start[*iter] <= begin]]
                            expl.push(getNegLeqLit(start[j], point_begin));
#ifdef VERBOSE_PROPAGATION_VERIFICATION
                            std::cout << "UB_start, " << j << ", " << point_begin << "; ";
#endif
                        }
                        if (dur[j]->getMin() > dur[j]->getMin0()) {
                            // Lower bound of the duration variable matters
                            expl.push(dur[j]->getMinLit());
#ifdef VERBOSE_PROPAGATION_VERIFICATION
                            std::cout << "LB_p, " << j << ", " << dur[j]->getMin() << "; ";
#endif
                        }
                        if (usage[j]->getMin() > usage[j]->getMin0()) {
                            // Lower bound of the usage variable matters
                            expl.push(usage[j]->getMinLit());
#ifdef VERBOSE_PROPAGATION_VERIFICATION
                            std::cout << "LB_c, " << j << ", " << usage[j]->getMin() << "; ";
#endif
                        }
                    }
#ifdef VERBOSE_PROPAGATION_VERIFICATION
                    std::cout << std::endl;
#endif
                }
                submit_conflict_explanation(expl);
                return false;
            }

            // Initialising the time point
            time_points[i].time = current_time;
            time_points[i].next_point_position = i + 1;
            if (i == 0) {
                time_points[i].previous_point_position = INT_MIN;
            }
            else {
                time_points[i].previous_point_position = i - 1;
            }
            time_points[i].over = 0;
            time_points[i].knapsack.reset_to_one();
            time_points[i].knapsack.new_max(limit - profile_height);
            time_points[i].req = 0;
            current_time = next_time;
            i++;
        }
        if (current_time < INT_MAX) {
            time_points[i].time = current_time;
            time_points[i].next_point_position = i + 1;
            if (i == 0) {
                time_points[i].previous_point_position = INT_MIN;
            }
            else {
                time_points[i].previous_point_position = i - 1;
            }
            time_points[i].over = 0;
            time_points[i].knapsack.reset_to_one();
            time_points[i].knapsack.new_max(limit);
            time_points[i].req = 0;
            i++;
        }
        // Setup a sentinel
        time_points[i].time = INT_MAX;
        time_points[i].next_point_position = INT_MAX;
        assert(i > 0);
        time_points[i].previous_point_position = i - 1;
        time_points[i].knapsack.reset_to_one();
        assert(profile_height == 0);
        time_points[i].knapsack.new_max(limit);
        time_points[i].over = 0;
        time_points[i].req = 0;
        assert(i < 4 * n_tasks + 1);

        first_relevant_tp = i;
        first_added_tp = i + 1;
        first_unused_added_tp = first_added_tp;
        return true;
    }

    // Returns the index of the time point corresponding to time. The time point must exist as a regular time point.
    // Complexity: O(log n)
    int find_time_point(int time) {
        int l = 0;
        int u = first_added_tp - 1;
        int m;
        while (l < u) {
            m = (l + u) / 2;
            if (time_points[m].time < time) {
                l = m + 1;
            }
            else {
                u = m;
            }
        }
        assert(time_points[l].time == time);
        return l;
    }

    // Sorts to_sort according to sort_ref
    static void sort_by_proxy(int* to_sort, const int* sort_ref, int size, bool insertion=false) {
        if (!insertion) {
            std::sort(to_sort, to_sort + size, [sort_ref](int i, int j) {return sort_ref[i] < sort_ref[j];});
        }
        else {
            int temp;
            for (int i = 1; i < size; i++) {
                for (int j = i; j > 0 && sort_ref[to_sort[j - 1]] > sort_ref[to_sort[j]]; j--) {
                    assert(0 <= to_sort[j] && to_sort[j] < size);
                    assert(0 <= to_sort[j - 1] && to_sort[j - 1] < size);
                    temp = to_sort[j];
                    to_sort[j] = to_sort[j - 1];
                    to_sort[j - 1] = temp;
                }
            }
        }
    }

    // Removes virtual time points from the structure
    // Complexity: O(n)
    void clear_virtual_tp() {
        assert(time_points[0].previous_point_position == INT_MIN);
        time_points[0].next_point_position = 1;
        for (int t = 1; t < first_added_tp - 1; t++) {
            time_points[t].previous_point_position = t - 1;
            time_points[t].next_point_position = t + 1;
        }
        assert(first_added_tp > 1);
        time_points[first_added_tp - 1].previous_point_position = first_added_tp - 2;
        assert(time_points[first_added_tp - 1].next_point_position == INT_MAX);

        first_unused_added_tp = first_added_tp;
    }

    // =================================================================================================================
    // Task set modification methods
    // =================================================================================================================
    // These methods change the set Theta used for the other methods. They have no effect on the profile heights.

    // Removes every task in the set
    // Complexity: O(Cn)
    void clear_set() {
        clear_virtual_tp();
        for (int t = 0; t < first_added_tp; t++) {
            time_points[t].req = 0;
            time_points[t].knapsack.reset_to_one();
            time_points[t].h_cons = 0;
            time_points[t].over = 0;
        }
    }

    // Adds a task in the set. The task is now accessible to knapsacks in [est_i, lct_i)\[lst_i, ect_i)).
    // Complexity: O(Cn)
    void add_task(int task, bool clear=true) {
        if (clear) {
            clear_virtual_tp();
        }

        int t = pos_est[task];
        int time = est[task];
        assert(time == time_points[t].time);
        while (time < lct[task]) {
            // The task is already forced in knapsacks where it has a compulsory part.
            assert(est[task] <= time);
            if (lst[task] > time || time >= ect[task]) {
                time_points[t].knapsack.add_item(usage[task]->getMin());
            }
            if (time < lst[task] && time < ect[task]){
                time_points[t].req += usage[task]->getMin();
            }
            t++;
            time = time_points[t].time;
        }
        if (est[task] != lst[task] && first_relevant_tp > pos_est[task]) {
            first_relevant_tp = pos_est[task];
        }
    }

    // Substitutes the set for the content of tasks_in_omega
    // Complexity: O(Cn^2)
    void fill_with_tasks() {
        clear_set();
        first_relevant_tp = first_added_tp - 1;
        if (tasks_in_omega.size() == 0) {
            return;
        }
        for (int i = 0; i < tasks_in_omega.size(); i++) {
            add_task(tasks_in_omega[i], false);
        }
    }

    // Substitutes the set for the LCut of i_lct. Also changes tasks_in_omega to that set.
    // Complexity: O(Cn^2)
    void fill_with_tasks(int i_lct) {
        tasks_in_omega.clear();
        for (int i = 0; i <= i_lct; i++) {
            tasks_in_omega.insert(ordered_lct[i]);
        }
        fill_with_tasks();
    }

    // =================================================================================================================
    // Profile modification methods
    // =================================================================================================================
    // These methods change the tasks considered for the profile heights. They have no effect on the inclusion of
    // virtual time points

    // Removes the effect of the task in the profile heights
    // Complexity: O(n)
    void remove_profile(int task) {
        assert(lst[task] < ect[task]);
        int t = find_time_point(lst[task]);
        int time = time_points[t].time;
        assert(time == lst[task]);
        int previous_cap;
        while (time < ect[task]) {
            assert(lst[task] <= time);
            previous_cap = time_points[t].knapsack.get_max();
            assert(limit - usage[task]->getMin() >= previous_cap);
            time_points[t].knapsack.new_max(previous_cap + usage[task]->getMin());
            t = time_points[t].next_point_position;
            time = time_points[t].time;
        }
    }

    // Adds back the effect of the task in the profile heights
    // Complexity: O(n)
    void add_profile(int task) {
        assert(lst[task] < ect[task]);
        int t = find_time_point(lst[task]);
        int time = time_points[t].time;
        assert(time == lst[task]);
        int previous_cap;
        while (time < ect[task]) {
            assert(lst[task] <= time);
            previous_cap = time_points[t].knapsack.get_max();
            assert(previous_cap >= usage[task]->getMin());
            time_points[t].knapsack.new_max(previous_cap - usage[task]->getMin());
            t = time_points[t].next_point_position;
            time = time_points[t].time;
        }
    }

    // Modify the effect of the task in the profile given new lst and ect bounds. Updates the values of est, ect, lst,
    // and lct. It is always a relaxation of the previous bounds. The new_bounds should already be time-points that exist.
    // Complexity: O(n)
    void adjust_profile(int task, int new_lst, int new_ect) {
        assert(lst[task] < ect[task]);
        assert(new_lst >= lst[task]);
        assert(new_ect <= ect[task]);
        assert(new_lst < new_ect);

        int t;
        int time;
        int previous_cap;
        if (new_ect < ect[task]) {
            t = find_time_point(ect[task]);
            assert(time_points[t].time == ect[task]);
            t = time_points[t].previous_point_position;
            time = time_points[t].time;
            while (time >= new_ect) {
                assert(ect[task] > time);
                previous_cap = time_points[t].knapsack.get_max();
                assert(limit >= previous_cap + usage[task]->getMin());
                time_points[t].knapsack.new_max(previous_cap + usage[task]->getMin());
                t = time_points[t].previous_point_position;
                time = time_points[t].time;
            }
            assert(time < new_ect);
            est[task] = new_ect - dur[task]->getMin();
            ect[task] = new_ect;
        }
        if (new_lst > lst[task]) {
            t = find_time_point(lst[task]);
            time = time_points[t].time;
            assert(time == lst[task]);
            while (time < new_lst) {
                assert(lst[task] <= time);
                previous_cap = time_points[t].knapsack.get_max();
                assert(limit >= previous_cap + usage[task]->getMin());
                time_points[t].knapsack.new_max(previous_cap + usage[task]->getMin());
                t = time_points[t].next_point_position;
                time = time_points[t].time;
            }
            assert(time == new_lst);
            lst[task] = new_lst;
            lct[task] = new_lst + dur[task]->getMin();
        }
    }

    // =================================================================================================================
    // KAOC implementation methods
    // =================================================================================================================
    // These methods are used to propagate the Knapsack-Augmented Overload Check rule.

    // Computes an approximation of the earliest completion time of Theta, as well as the overloading energy that cannot
    // be used in [est_theta, lct_theta).
    // Complexity: O(Cn)
    void schedule_tasks_kaoc(int& ect_theta, int lct_theta, int& over, bool no_addition=false) {
        int current_t = first_relevant_tp;
        int next_t;
        int time = time_points[current_t].time;
        over = 0;
        ect_theta = INT_MIN;
        int l;
        int h_req;
        int h_cons;
        while (time < lct_theta) {
            next_t = time_points[current_t].next_point_position;

            time_points[current_t].over = over;
            l = time_points[next_t].time - time;
            h_req = time_points[current_t].req;
            h_cons = std::min(h_req + over, time_points[current_t].knapsack.find_msb());
            if (!no_addition) {
                if (0 < over && over < (h_cons - h_req) * l) {
                    assert(first_unused_added_tp < 5 * n_tasks + 1);
                    l = std::max(1, over / (h_cons - h_req));
                    time_points[first_unused_added_tp].time = time + l;
                    time_points[first_unused_added_tp].next_point_position = time_points[current_t].next_point_position;
                    time_points[first_unused_added_tp].previous_point_position = current_t;
                    time_points[first_unused_added_tp].knapsack.copy_values(time_points[current_t].knapsack);
                    time_points[first_unused_added_tp].knapsack.new_max(time_points[current_t].knapsack.get_max());
                    time_points[first_unused_added_tp].req = time_points[current_t].req;
                    time_points[first_unused_added_tp].over = 0;

                    time_points[current_t].next_point_position = first_unused_added_tp;
                    time_points[next_t].previous_point_position = first_unused_added_tp;
                    next_t = first_unused_added_tp;
                    first_unused_added_tp++;
                }
                over += (h_req - h_cons) * l;
            }
            else {
                over = std::max(0, over + ((h_req - h_cons) * l));
            }
            time_points[current_t].over = over;
            if (h_cons > 0) {
                ect_theta = time_points[next_t].time;
            }

            current_t = next_t;
            time = time_points[current_t].time;
        }
    }

    // Applies the Knapsack-Augmented Overload Check rule
    // Complexity: O(Cn^2 + explanation complexity)
    bool knapsack_augmented_overload_check() {
        if (!init_structure()) {
            return false;
        }

        int ect_theta;
        int lct_theta;
        int over;
        int e_theta = 0;
        int est_theta = INT_MAX;

        for (int i = 0; i < n_tasks; i++) {
            add_task(ordered_lct[i]);
            est_theta = std::min(est_theta, est[ordered_lct[i]]);
            e_theta += dur[ordered_lct[i]]->getMin() * usage[ordered_lct[i]]->getMin();
            lct_theta = lct[ordered_lct[i]];
            schedule_tasks_kaoc(ect_theta, lct_theta, over);
            if (ect_theta > lct_theta || over > 0) {
                vec<Lit> expl;
#ifdef VERBOSE_PROPAGATION_VERIFICATION
                report_propagation("Conflict (KAOC)");
#endif
                if (so.lazy) {
                    fill_kaoc_explanation(expl, i, over);
                }
                submit_conflict_explanation(expl);
                return false;
            }

            assert(e_theta <= limit * (lct_theta - est_theta));
        }
        return true;
    }

    void insert_after(int previous_point, int position, int time) {
        assert(time_points[previous_point].time < time);
        assert(time_points[previous_point].next_point_position == INT_MAX || time < time_points[time_points[previous_point].next_point_position].time);
        time_points[position].time = time;
        time_points[position].previous_point_position = previous_point;
        assert(time_points[previous_point].next_point_position != position);
        time_points[position].next_point_position = time_points[previous_point].next_point_position;
        time_points[time_points[position].next_point_position].previous_point_position = position;
        time_points[previous_point].next_point_position = position;
        time_points[position].req = time_points[previous_point].req;
        time_points[position].knapsack.copy_values(time_points[previous_point].knapsack);
        time_points[position].knapsack.new_max(time_points[previous_point].knapsack.get_max());
    }

    void remove_point(int position) {
        if (time_points[position].previous_point_position != INT_MIN) {
            time_points[time_points[position].previous_point_position].next_point_position = time_points[position].next_point_position;
        }
        if (time_points[position].next_point_position != INT_MAX) {
            time_points[time_points[position].next_point_position].previous_point_position = time_points[position].previous_point_position;
        }
    }

    // Attempt to slack the task from the set Theta
    // Complexity: O(Cn^2)
    void slack_task(int task, int lct_set, int& over) {
        assert(tasks_in_omega.contains(task));
        if (est[task] == lst[task]) {
            // The task is fixed and will always be removed
            assert(ect[task] == lct[task]);
            tasks_in_omega.remove(task);
        }
        int t = pos_est[task];
        int other_task;
        while (time_points[t].time < lct[task]) {
            assert(est[task] <= time_points[t].time && time_points[t].time < lct[task]);
            if (lst[task] > time_points[t].time || time_points[t].time >= ect[task]) {
                time_points[t].knapsack.reset_to_one();
                for (int i = 0; i < tasks_in_omega.size(); i++) {
                    other_task = tasks_in_omega[i];
                    if (task == other_task) {
                        continue;
                    }
                    if (est[other_task] <= time_points[t].time && time_points[t].time < lct[other_task] && (lst[other_task] > time_points[t].time || time_points[t].time >= ect[other_task])) {
                        time_points[t].knapsack.add_item(usage[other_task]->getMin());
                    }
                }
                if (time_points[t].time < std::min(lst[task], ect[task])) {
                    time_points[t].req -= usage[task]->getMin();
                }
            }
            t = time_points[t].next_point_position;
        }
        int new_over;
        int dump;
        schedule_tasks_kaoc(dump, lct_set, new_over);
        if (new_over > 0) {
            over = new_over;
            tasks_in_omega.remove(task);
        }
        else {
            t = pos_est[task];
            while (time_points[t].time < lct[task]) {
                assert(est[task] <= time_points[t].time && time_points[t].time < lct[task]);
                if (lst[task] > time_points[t].time || time_points[t].time >= ect[task]) {
                    time_points[t].knapsack.add_item(usage[task]->getMin());
                    if (time_points[t].time < std::min(lst[task], ect[task])) {
                        time_points[t].req += usage[task]->getMin();
                    }
                }
                t = time_points[t].next_point_position;
            }
        }
    }

    // Lift the est of a task for the explanations
    // Complexity: O(Cn^2)
    void lift_est(int task, int est_set, int lct_set, int& over) {
        assert(tasks_in_omega.contains(task) || tasks_in_profile.contains(task));
        int min_est;
        if (tasks_in_omega.contains(task)) {
            min_est = std::max(time_points[0].time, start[task]->getMin0());
        }
        else {
            min_est = std::max(std::max(lst[task] - dur[task]->getMin(), time_points[0].time), start[task]->getMin0());
        }
        int new_est = est[task];
        assert(new_est >= min_est || !tasks_in_omega.contains(task));
        if (new_est <= min_est) {
            return;
        }

        int t_iter_est = time_points[pos_est[task]].previous_point_position;
        assert(time_points[t_iter_est].time < new_est && time_points[time_points[t_iter_est].next_point_position].time >= new_est);
        int t_iter_ect = t_iter_est;
        while (time_points[t_iter_ect].time < new_est + dur[task]->getMin()) {
            t_iter_ect = time_points[t_iter_ect].next_point_position;
        }
        t_iter_ect = time_points[t_iter_ect].previous_point_position;
        assert(time_points[t_iter_ect].time < new_est + dur[task]->getMin() && time_points[time_points[t_iter_ect].next_point_position].time >= new_est + dur[task]->getMin());

        int virt_est_iter = (6 * n_tasks) + 1;
        BitSet<uint_bitset, is_knapsack> saved_knapsack1(limit + 1, limit);
        int virt_ect_iter = (6 * n_tasks) + 2;
        BitSet<uint_bitset, is_knapsack> saved_knapsack2(limit + 1, limit);

        int step;
        int ect_theta;
        int new_over;

        // This is so schedule_tasks_kaoc remains valid. It may make the function look at some useless points.
        first_relevant_tp = 0;
        do {
            assert(t_iter_est >= 0 && t_iter_ect >= 0);
            step = std::min(new_est - time_points[t_iter_est].time, new_est + dur[task]->getMin() - time_points[t_iter_ect].time);
            assert(step > 0);
            assert(time_points[t_iter_est].time == new_est - step || time_points[t_iter_ect].time == new_est + dur[task]->getMin() - step);

            if (time_points[t_iter_est].time < new_est - step) {
                // At virtual time point must be added to the timeline before the current est
                time_points[virt_est_iter].time = new_est - step;
                time_points[virt_est_iter].previous_point_position = t_iter_est;
                if (virt_est_iter != time_points[t_iter_est].next_point_position) {
                    time_points[virt_est_iter].next_point_position = time_points[t_iter_est].next_point_position;
                    time_points[time_points[virt_est_iter].next_point_position].previous_point_position = virt_est_iter;
                }
                time_points[t_iter_est].next_point_position = virt_est_iter;
                time_points[virt_est_iter].knapsack.copy_values(time_points[t_iter_est].knapsack);
                time_points[virt_est_iter].knapsack.new_max(time_points[t_iter_est].knapsack.get_max());
                if (tasks_in_omega.contains(task)) {
                    time_points[virt_est_iter].req = time_points[t_iter_est].req + usage[task]->getMin();
                    time_points[virt_est_iter].knapsack.add_item(usage[task]->getMin());
                }
                else {
                    time_points[virt_est_iter].req = time_points[t_iter_est].req;
                }
                assert(verif_time_point_chain());
            }
            else if (tasks_in_omega.contains(task)) {
                // No new time point is necessary before the current est
                // The previous knapsack must however be saved to help revert changes
                time_points[t_iter_est].req += usage[task]->getMin();
                saved_knapsack1.copy_values(time_points[t_iter_est].knapsack);
                time_points[t_iter_est].knapsack.add_item(usage[task]->getMin());
                assert(verif_time_point_chain());
            }

            if (time_points[t_iter_ect].time < new_est + dur[task]->getMin() - step) {
                // At virtual time point must be added to the timeline before the current ect
                time_points[virt_ect_iter].time = new_est + dur[task]->getMin() - step;
                time_points[virt_ect_iter].previous_point_position = t_iter_ect;
                if (virt_ect_iter != time_points[t_iter_ect].next_point_position) {
                    time_points[virt_ect_iter].next_point_position = time_points[t_iter_ect].next_point_position;
                    time_points[time_points[virt_ect_iter].next_point_position].previous_point_position = virt_ect_iter;
                }
                time_points[t_iter_ect].next_point_position = virt_ect_iter;
                time_points[virt_ect_iter].req = time_points[t_iter_ect].req;
                time_points[virt_ect_iter].knapsack.copy_values(time_points[t_iter_ect].knapsack);
                time_points[virt_ect_iter].knapsack.new_max(time_points[t_iter_ect].knapsack.get_max());

                if (lst[task] < new_est + dur[task]->getMin()) {
                    // The task has a compulsory part that get relaxed
                    assert(lst[task] <= time_points[virt_ect_iter].time);
                    if (tasks_in_omega.contains(task)) {
                        time_points[virt_ect_iter].knapsack.add_item(usage[task]->getMin());
                    }
                    time_points[virt_ect_iter].knapsack.new_max(time_points[virt_ect_iter].knapsack.get_max() + usage[task]->getMin());
                }
                else if (tasks_in_omega.contains(task)) {
                    // There was no compulsory part
                    time_points[virt_ect_iter].req -= usage[task]->getMin();
                }
                assert(verif_time_point_chain());
            }
            else {
                // No new time point is necessary before the current est
                if (lst[task] < new_est + dur[task]->getMin()) {
                    // The task has a compulsory part that get relaxed
                    // The previous knapsack must be saved to help revert changes
                    assert(lst[task] <= time_points[t_iter_ect].time);
                    if (tasks_in_omega.contains(task)) {
                        saved_knapsack2.copy_values(time_points[t_iter_ect].knapsack);
                        time_points[t_iter_ect].knapsack.add_item(usage[task]->getMin());
                    }
                    time_points[t_iter_ect].knapsack.new_max(time_points[t_iter_ect].knapsack.get_max() + usage[task]->getMin());
                }
                else if (tasks_in_omega.contains(task)) {
                    // There was no compulsory part
                    time_points[t_iter_ect].req -= usage[task]->getMin();
                }
                assert(verif_time_point_chain());
            }

            schedule_tasks_kaoc(ect_theta, lct_set, new_over, true);
            if (ect_theta > lct_set || new_over > 0) {
                over = new_over;
                assert(time_points[t_iter_est].time <= new_est - step);
                if (time_points[t_iter_est].time == new_est - step) {
                    t_iter_est = time_points[t_iter_est].previous_point_position;
                    assert(verif_time_point_chain());
                }
                else {
                    // We still remove the virtual point
                    time_points[t_iter_est].next_point_position = time_points[virt_est_iter].next_point_position;
                    if (time_points[t_iter_est].next_point_position != INT_MAX) {
                        time_points[time_points[t_iter_est].next_point_position].previous_point_position = t_iter_est;
                    }
                    assert(verif_time_point_chain());
                }
                assert(time_points[t_iter_ect].time <= new_est + dur[task]->getMin() - step);
                if (time_points[t_iter_ect].time == new_est + dur[task]->getMin() - step) {
                    t_iter_ect = time_points[t_iter_ect].previous_point_position;
                    assert(verif_time_point_chain());
                }
                else {
                    // We still remove the virtual point
                    time_points[t_iter_ect].next_point_position = time_points[virt_ect_iter].next_point_position;
                    if (time_points[t_iter_ect].next_point_position != INT_MAX) {
                        time_points[time_points[t_iter_ect].next_point_position].previous_point_position = t_iter_ect;
                    }
                    assert(verif_time_point_chain());
                }
                new_est -= step;
            }
            else {
                // The conflict is no longer detected
                // We must revert all changes in the timeline
                assert(time_points[t_iter_est].time <= new_est - step);
                if (time_points[t_iter_est].time < new_est - step) {
                    // It is sufficient to extract the virtual time point
                    time_points[t_iter_est].next_point_position = time_points[virt_est_iter].next_point_position;
                    if (time_points[t_iter_est].next_point_position != INT_MAX) {
                        time_points[time_points[t_iter_est].next_point_position].previous_point_position = t_iter_est;
                    }
                    assert(verif_time_point_chain());
                }
                else if (tasks_in_omega.contains(task)) {
                    // Changes must be reverted
                    time_points[t_iter_est].req -= usage[task]->getMin();
                    time_points[t_iter_est].knapsack.copy_values(saved_knapsack1);
                    assert(verif_time_point_chain());
                }

                assert(time_points[t_iter_ect].time <= new_est + dur[task]->getMin() - step);
                if (time_points[t_iter_ect].time < new_est + dur[task]->getMin() - step) {
                    // It is sufficient to extract the virtual time point
                    time_points[t_iter_ect].next_point_position = time_points[virt_ect_iter].next_point_position;
                    if (time_points[t_iter_ect].next_point_position != INT_MAX) {
                        time_points[time_points[t_iter_ect].next_point_position].previous_point_position = t_iter_ect;
                    }
                    assert(verif_time_point_chain());
                }
                else {
                    // Changes must be reverted
                    if (lst[task] < new_est + dur[task]->getMin()) {
                        if (tasks_in_omega.contains(task)) {
                            time_points[t_iter_ect].knapsack.copy_values(saved_knapsack2);
                        }
                        time_points[t_iter_ect].knapsack.new_max(time_points[t_iter_ect].knapsack.get_max() - usage[task]->getMin());
                    }
                    else if (tasks_in_omega.contains(task)) {
                        time_points[t_iter_ect].req += usage[task]->getMin();
                    }
                    assert(verif_time_point_chain());
                }

                step = 0;
            }

        } while(new_est > min_est && step > 0);

        est[task] = new_est;
        ect[task] = est[task] + dur[task]->getMin();

        assert(verif_time_point_chain());
        if (t_iter_est == INT_MIN) {
           assert(new_est == min_est && min_est == time_points[0].time);
        }
        else {
            assert(time_points[time_points[t_iter_est].next_point_position].time == est[task] || time_points[time_points[t_iter_ect].next_point_position].time == ect[task]);
        }

        if (t_iter_est != INT_MIN && time_points[time_points[t_iter_est].next_point_position].time != est[task]) {
            // We must add the time point
            if (tasks_in_omega.contains(task)) {
                assert(time_points[t_iter_est].time < est[task] && time_points[time_points[t_iter_est].next_point_position].time > est[task]);
                time_points[first_unused_added_tp].time = est[task];
                time_points[first_unused_added_tp].previous_point_position = t_iter_est;
                time_points[first_unused_added_tp].next_point_position = time_points[t_iter_est].next_point_position;
                time_points[t_iter_est].next_point_position = first_unused_added_tp;
                time_points[time_points[first_unused_added_tp].next_point_position].previous_point_position = first_unused_added_tp;
                time_points[first_unused_added_tp].req = time_points[t_iter_est].req + usage[task]->getMin();
                time_points[first_unused_added_tp].knapsack.copy_values(time_points[t_iter_est].knapsack);
                time_points[first_unused_added_tp].knapsack.add_item(usage[task]->getMin());
                time_points[first_unused_added_tp].knapsack.new_max(time_points[t_iter_est].knapsack.get_max());

                pos_est[task] = first_unused_added_tp;
                if (t_iter_ect == t_iter_est) {
                    t_iter_ect = first_unused_added_tp;
                    assert(time_points[t_iter_ect].time < ect[task] && time_points[time_points[t_iter_ect].next_point_position].time >= ect[task]);
                }
                first_unused_added_tp++;
                assert(verif_time_point_chain());
            }
        }
        else {
            if (t_iter_est != INT_MIN) {
                pos_est[task] = time_points[t_iter_est].next_point_position;
            }
            else {
                pos_est[task] = 0;
            }
            assert(verif_time_point_chain());
        }
        if (time_points[time_points[t_iter_ect].next_point_position].time != ect[task]) {
            // We must add the time point
            assert(time_points[t_iter_ect].time < ect[task] && time_points[time_points[t_iter_ect].next_point_position].time > ect[task]);
            time_points[first_unused_added_tp].time = ect[task];
            time_points[first_unused_added_tp].previous_point_position = t_iter_ect;
            time_points[first_unused_added_tp].next_point_position = time_points[t_iter_ect].next_point_position;
            time_points[t_iter_ect].next_point_position = first_unused_added_tp;
            time_points[time_points[first_unused_added_tp].next_point_position].previous_point_position = first_unused_added_tp;
            time_points[first_unused_added_tp].req = time_points[t_iter_ect].req;
            time_points[first_unused_added_tp].knapsack.copy_values(time_points[t_iter_ect].knapsack);
            time_points[first_unused_added_tp].knapsack.new_max(time_points[t_iter_ect].knapsack.get_max());
            if (lst[task] < ect[task]) {
                if (tasks_in_omega.contains(task)) {
                    time_points[first_unused_added_tp].knapsack.add_item(usage[task]->getMin());
                }
                time_points[first_unused_added_tp].knapsack.new_max(time_points[first_unused_added_tp].knapsack.get_max() + usage[task]->getMin());
            }
            else if (tasks_in_omega.contains(task)) {
                time_points[first_unused_added_tp].req -= usage[task]->getMin();
            }

            first_unused_added_tp++;
            assert(verif_time_point_chain());
        }
    }

    // Lift the lst of a task for the explanations
    // Complexity: O(Cn^2)
    void lift_lst(int task, int& lct_set, int& over) {
        assert(tasks_in_omega.contains(task) || tasks_in_profile.contains(task));
        int max_lst;
        if (tasks_in_omega.contains(task)) {
            max_lst = std::min(time_points[time_points[first_added_tp - 1].previous_point_position].time, start[task]->getMax0());
        }
        else {
            max_lst = std::min(ect[task], start[task]->getMax0());
        }
        int new_lst = lst[task];
        assert(new_lst <= max_lst || !tasks_in_omega.contains(task));
        if (new_lst >= max_lst) {
            return;
        }

        int t_iter_lst = 0;
        while (time_points[t_iter_lst].time <= new_lst) {
            t_iter_lst = time_points[t_iter_lst].next_point_position;
        }
        assert(time_points[time_points[t_iter_lst].previous_point_position].time <= new_lst && time_points[t_iter_lst].time > new_lst);
        int t_iter_lct = t_iter_lst;
        while (time_points[t_iter_lct].time <= new_lst + dur[task]->getMin()) {
            t_iter_lct = time_points[t_iter_lct].next_point_position;
        }
        assert(time_points[time_points[t_iter_lct].previous_point_position].time <= new_lst + dur[task]->getMin() && time_points[t_iter_lct].time > new_lst + dur[task]->getMin());

        int virt_lst_iter = (6 * n_tasks) + 1;
        BitSet<uint_bitset, is_knapsack> saved_knapsack1(limit + 1, limit);
        int virt_lct_iter = (6 * n_tasks) + 2;
        BitSet<uint_bitset, is_knapsack> saved_knapsack2(limit + 1, limit);

        int step;
        int ect_theta;
        int new_over;
        do {
            assert(time_points[time_points[t_iter_lst].previous_point_position].time <= new_lst && time_points[t_iter_lst].time > new_lst);
            assert(time_points[time_points[t_iter_lct].previous_point_position].time <= new_lst + dur[task]->getMin() && time_points[t_iter_lct].time > new_lst + dur[task]->getMin());
            step = std::min(time_points[t_iter_lst].time - new_lst, time_points[t_iter_lct].time - new_lst - dur[task]->getMin());
            assert(step > 0);
            assert(time_points[t_iter_lst].time == new_lst + step || time_points[t_iter_lct].time == new_lst + dur[task]->getMin() + step);

            if (time_points[time_points[t_iter_lst].previous_point_position].time == new_lst && new_lst < ect[task]) {
                time_points[time_points[t_iter_lst].previous_point_position].knapsack.new_max(time_points[time_points[t_iter_lst].previous_point_position].knapsack.get_max() + usage[task]->getMin());
                if (tasks_in_omega.contains(task)) {
                    saved_knapsack1.copy_values(time_points[time_points[t_iter_lst].previous_point_position].knapsack);
                    time_points[time_points[t_iter_lst].previous_point_position].knapsack.add_item(usage[task]->getMin());
                    time_points[time_points[t_iter_lst].previous_point_position].req += usage[task]->getMin();
                }
            }
            if (time_points[t_iter_lst].time > new_lst + step) {
                // At virtual time point must be added to the timeline after the current lst
                insert_after(time_points[t_iter_lst].previous_point_position, virt_lst_iter, new_lst + step);
                if (new_lst < ect[task]) {
                    // The relaxation on the previous time point must be reverted
                    time_points[virt_lst_iter].knapsack.new_max(time_points[virt_lst_iter].knapsack.get_max() - usage[task]->getMin());
                    if (tasks_in_omega.contains(task)) {
                        time_points[virt_lst_iter].knapsack.copy_values(saved_knapsack1);
                        time_points[virt_lst_iter].req -= usage[task]->getMin();
                    }
                }
                assert(verif_time_point_chain());
            }

            if (t_iter_lct != t_iter_lst) {
                if (tasks_in_omega.contains(task) && time_points[time_points[t_iter_lct].previous_point_position].time == new_lst + dur[task]->getMin()) {
                    saved_knapsack2.copy_values(time_points[time_points[t_iter_lct].previous_point_position].knapsack);
                    time_points[time_points[t_iter_lct].previous_point_position].knapsack.add_item(usage[task]->getMin());
                }
                if (time_points[t_iter_lct].time > new_lst + dur[task]->getMin() + step) {
                    // A virtual time point must be added to the timeline before the current ect
                    insert_after(time_points[t_iter_lct].previous_point_position, virt_lct_iter, new_lst + dur[task]->getMin() + step);
                    if (tasks_in_omega.contains(task)) {
                        // The relaxation on the previous time point must be reverted
                        time_points[virt_lct_iter].knapsack.copy_values(saved_knapsack2);
                    }
                    assert(verif_time_point_chain());
                }
            }


            schedule_tasks_kaoc(ect_theta, std::max(lct_set, new_lst + dur[task]->getMin() + step), new_over, true);
            if (ect_theta > std::max(lct_set, new_lst + dur[task]->getMin() + step) || new_over > 0) {
                over = new_over;
                assert(time_points[t_iter_lct].time >= new_lst + dur[task]->getMin() + step);
                if (time_points[t_iter_lct].time == new_lst + dur[task]->getMin() + step) {
                    t_iter_lct = time_points[t_iter_lct].next_point_position;
                    assert(verif_time_point_chain());
                }
                else {
                    // We still remove the virtual point
                    remove_point(virt_lct_iter);
                    assert(verif_time_point_chain());
                }
                assert(time_points[t_iter_lst].time >= new_lst + step);
                if (time_points[t_iter_lst].time == new_lst + step) {
                    t_iter_lst = time_points[t_iter_lst].next_point_position;
                    assert(verif_time_point_chain());
                }
                else {
                    // We still remove the virtual point
                    remove_point(virt_lst_iter);
                    assert(verif_time_point_chain());
                }
                new_lst += step;
            }
            else {
                // The conflict is no longer detected
                // We must revert all changes in the timeline
                assert(time_points[t_iter_lst].time >= new_lst + step);
                if (time_points[t_iter_lst].time > new_lst + step) {
                    // We must extract the virtual time point
                    remove_point(virt_lst_iter);
                    assert(verif_time_point_chain());
                }
                if (time_points[time_points[t_iter_lst].previous_point_position].time == new_lst && new_lst < ect[task]) {
                    time_points[time_points[t_iter_lst].previous_point_position].knapsack.new_max(time_points[time_points[t_iter_lst].previous_point_position].knapsack.get_max() - usage[task]->getMin());
                    if (tasks_in_omega.contains(task)) {
                        time_points[time_points[t_iter_lst].previous_point_position].knapsack.copy_values(saved_knapsack1);
                        time_points[time_points[t_iter_lst].previous_point_position].req -= usage[task]->getMin();
                    }
                }

                assert(time_points[t_iter_lct].time >= new_lst + dur[task]->getMin() + step);
                if (time_points[t_iter_lct].time > new_lst + dur[task]->getMin() + step) {
                    // We must extract the virtual time point
                    remove_point(virt_lct_iter);
                    assert(verif_time_point_chain());
                }
                if (tasks_in_omega.contains(task) && time_points[time_points[t_iter_lct].previous_point_position].time == new_lst) {
                    time_points[time_points[t_iter_lct].previous_point_position].knapsack.copy_values(saved_knapsack2);
                }

                step = 0;
            }

        } while(new_lst < max_lst && step > 0);

        lst[task] = new_lst;
        lct[task] = lst[task] + dur[task]->getMin();
        lct_set = std::max(lct_set, lct[task]);

        assert(verif_time_point_chain());
        assert(time_points[time_points[t_iter_lst].previous_point_position].time == lst[task] || time_points[time_points[t_iter_lct].previous_point_position].time == lct[task]);

        if (time_points[time_points[t_iter_lst].previous_point_position].time != lst[task]) {
            // We must add the time point
            assert(time_points[time_points[t_iter_lst].previous_point_position].time < lst[task] && time_points[t_iter_lst].time > lst[task]);
            insert_after(time_points[t_iter_lst].previous_point_position, first_unused_added_tp, lst[task]);
            first_unused_added_tp++;
            assert(verif_time_point_chain());
        }
        assert(verif_time_point_chain());
        if (tasks_in_omega.contains(task) && time_points[time_points[t_iter_lct].previous_point_position].time != lct[task]) {
            // We must add the time point
            assert(time_points[time_points[t_iter_lct].previous_point_position].time < lct[task] && time_points[t_iter_lct].time > lct[task]);
            insert_after(time_points[t_iter_lct].previous_point_position, first_unused_added_tp, lct[task]);
            first_unused_added_tp++;
            assert(verif_time_point_chain());
        }
    }

    // Computes the explanation for the detected Knapsack-Augmented Overload Check conflict
    // Complexity: O(Cn^3)
    void fill_kaoc_explanation(vec<Lit>& expl, int task_lct, int over) {
        int dump1;
        int old_over;

        int lct_set = lct[ordered_lct[task_lct]];

        int est_set = time_points[first_relevant_tp].time;
        int pseudo_est_set;

        int diff_over;

        assert(verif_order());

        tasks_in_omega.clear();
        tasks_in_profile.clear();
        int task;
        for (int i = task_lct; i >= 0; i--) {
            task = ordered_lct[i];
            if (est[task] < est_set) {
                continue;
            }
            tasks_in_omega.insert(task);
        }

        assert(tasks_in_omega.size() > 0);
        for (int j = 0; j < tasks_in_omega.size(); j++) {
            assert(lct[tasks_in_omega[j]] <= lct_set);
        }

        for (int i = 0; i < n_tasks; i++) {
            task = ordered_est[i];
            if (tasks_in_omega.contains(task)) {
                slack_task(task, lct_set, over);
            }
        }

        assert(tasks_in_omega.size() > 0);
        est_set = INT_MAX;
        lct_set = INT_MIN;
        for (int i = 0; i < tasks_in_omega.size(); i++) {
            est_set = std::min(est_set, est[tasks_in_omega[i]]);
	    lct_set = std::max(lct_set, lct[tasks_in_omega[i]]);
        }

        for (int i = 0; i < n_tasks; i++) {
            task = i;
            if (std::min(lct_set, ect[task]) > std::max(est_set, lst[task])) {
                tasks_in_profile.insert(task);
            }
            else if (ect[task] > lst[task]) {
                remove_profile(task);
            }
        }

        if (!kaoc_expl_lifting) {
            for (int i = 0; i < tasks_in_omega.size(); i++) {
                naively_explain_task(expl, tasks_in_omega[i]);
            }
            for (int i = 0; i < tasks_in_profile.size(); i++) {
                task = tasks_in_profile[i];
                if (!tasks_in_omega.contains(task)) {
                    naively_explain_task(expl, tasks_in_profile[i]);
                }
            }
        }
        else {
            assert(over > 0);
            // Lifting tasks only in the profile (out of Theta)
            for (int i = 0; i < n_tasks; i++) {
                assert(over > 0);
                if (!tasks_in_profile.contains(i)) {
                    continue;
                }
                task = i;
                if (tasks_in_omega.contains(task)) {
                    continue;
                }
                lift_est(task, est_set, lct_set, over);
                lift_lst(task, lct_set, over);

                if (lst[task] < ect[task]) {
                    if (ect[task] - dur[task]->getMin()  > start[task]->getMin0()) {
                        expl.push(getNegGeqLit(start[task], ect[task] - dur[task]->getMin()));
#ifdef VERBOSE_PROPAGATION_VERIFICATION
                        std::cout << "LB_start, " << task << ", " << ect[task] - dur[task]->getMin() << "; ";
#endif
                    }
                    if (lst[task] < start[task]->getMax0()) {
                        expl.push(getNegLeqLit(start[task], lst[task]));
#ifdef VERBOSE_PROPAGATION_VERIFICATION
                        std::cout << "UB_start, " << task << ", " << lst[task] << "; ";
#endif
                    }
                    if (dur[task]->getMin() > dur[task]->getMin0()) {
                        // Lower bound of the duration variable matters
                        expl.push(dur[task]->getMinLit());
#ifdef VERBOSE_PROPAGATION_VERIFICATION
                        std::cout << "LB_p, " << task << ", " << dur[task]->getMin() << "; ";
#endif
                    }
                    if (usage[task]->getMin() > usage[task]->getMin0()) {
                        // Lower bound of the usage variable matters
                        expl.push(usage[task]->getMinLit());
#ifdef VERBOSE_PROPAGATION_VERIFICATION
                        std::cout << "LB_c, " << task << ", " << usage[task]->getMin() << "; ";
#endif
                    }
                }
            }

//          Lift and explain the est of tasks in Theta
            for (int i = n_tasks - 1; i >= 0; i--) {
                task = ordered_lct[i];
                if (!tasks_in_omega.contains(task)) {
                    continue;
                }
                lift_est(task, est_set, lct_set, over);

                if (est[task] > start[task]->getMin0()) {
                    expl.push(getNegGeqLit(start[task], est[task]));
#ifdef VERBOSE_PROPAGATION_VERIFICATION
                    std::cout << "LB_start, " << task << ", " << est[task] << "; ";
#endif
                }
                if (dur[task]->getMin() > dur[task]->getMin0()) {
                    // Lower bound of the duration variable matters
                    expl.push(dur[task]->getMinLit());
#ifdef VERBOSE_PROPAGATION_VERIFICATION
                    std::cout << "LB_p, " << task << ", " << dur[task]->getMin() << "; ";
#endif
                }
                if (usage[task]->getMin() > usage[task]->getMin0()) {
                    // Lower bound of the usage variable matters
                    expl.push(usage[task]->getMinLit());
#ifdef VERBOSE_PROPAGATION_VERIFICATION
                    std::cout << "LB_c, " << task << ", " << usage[task]->getMin() << "; ";
#endif
                }
            }

//          Lift and explain the lst of tasks in Theta
            for (int i = n_tasks - 1; i >= 0; i--) {
                task = ordered_lct[i];
                if (!tasks_in_omega.contains(task)) {
                    continue;
                }
                lift_lst(task, lct_set, over);

                if (lst[task] < start[task]->getMax0()) {
                    expl.push(getNegLeqLit(start[task], lst[task]));
#ifdef VERBOSE_PROPAGATION_VERIFICATION
                    std::cout << "UB_start, " << task << ", " << lst[task] << "; ";
#endif
                }
            }
        }
#ifdef VERBOSE_PROPAGATION_VERIFICATION
        std::cout << std::endl;
#endif
    }

#ifdef VERBOSE_PROPAGATION_VERIFICATION
    // =================================================================================================================
    // Brute force validation methods
    // =================================================================================================================
    // These methods report propagations to the validation script

    void present_propagator() {
        std::cout << "Constraint: Cumulative " << prop_id << "\n";

        std::cout << "LB_start: ";
        for (int i = 0; i < n_tasks; i++) {
            std::cout << start[i]->getMin0();
            if (i < n_tasks - 1) {
                std::cout << ", ";
            }
        }
        std::cout << "\n";

        std::cout << "UB_start: ";
        for (int i = 0; i < n_tasks; i++) {
            std::cout << start[i]->getMax0();
            if (i < n_tasks - 1) {
                std::cout << ", ";
            }
        }
        std::cout << "\n";

        std::cout << "LB_p: ";
        for (int i = 0; i < n_tasks; i++) {
            std::cout << dur[i]->getMin0();
            if (i < n_tasks - 1) {
                std::cout << ", ";
            }
        }
        std::cout << "\n";

        std::cout << "UB_p: ";
        for (int i = 0; i < n_tasks; i++) {
            std::cout << dur[i]->getMax0();
            if (i < n_tasks - 1) {
                std::cout << ", ";
            }
        }
        std::cout << "\n";

        std::cout << "LB_c: ";
        for (int i = 0; i < n_tasks; i++) {
            std::cout << usage[i]->getMin0();
            if (i < n_tasks - 1) {
                std::cout << ", ";
            }
        }
        std::cout << "\n";

        std::cout << "UB_c: ";
        for (int i = 0; i < n_tasks; i++) {
            std::cout << usage[i]->getMax0();
            if (i < n_tasks - 1) {
                std::cout << ", ";
            }
        }
        std::cout << "\n";

        std::cout << "C: " << limit << std::endl;
    }

    void report_propagation(const std::string& s) {
        if (!present) {
            std::cout << "***" << "\n";
            std::cout << "Constraint: Cumulative " << prop_id << "\n";
            present = true;
        }
        std::cout << "!!!" << "\n";
        std::cout << s << "\n";
        std::cout << "Explanation: ";
    }
#endif

    bool verif_order() {
        for (int i = 0; i < n_tasks - 1; i++) {
            if (est[ordered_est[i]] > est[ordered_est[i + 1]] ||
                lst[ordered_lst[i]] > lst[ordered_lst[i + 1]] ||
                ect[ordered_ect[i]] > ect[ordered_ect[i + 1]] ||
                lct[ordered_lct[i]] > lct[ordered_lct[i + 1]]) {
                return false;
            }
        }
        return true;
    }

    bool verif_time_point_chain() {
        int i = 0;
        int counter = 0;
        while (i != first_added_tp - 1) {
            if (counter > 6 * n_tasks + 3) {
                return false;
            }
            if (0 != i && time_points[i].previous_point_position == INT_MIN) {
                return false;
            }
            if (time_points[i].time == INT_MAX || time_points[i].next_point_position == INT_MAX || time_points[time_points[i].next_point_position].previous_point_position != i) {
                return false;
            }
            i = time_points[i].next_point_position;
            counter++;
        }
        return time_points[i].time == INT_MAX && time_points[i].next_point_position == INT_MAX;
    }

    // =================================================================================================================
    // Explanation creation helper methods
    // =================================================================================================================

    void naively_explain_task(vec<Lit>& expl, int task) {
        if (start[task]->getMin() > start[task]->getMin0()) {
            expl.push(start[task]->getMinLit());
#ifdef VERBOSE_PROPAGATION_VERIFICATION
            std::cout << "LB_start, " << task << ", " << start[task]->getMin() << "; ";
#endif
        }
        if (start[task]->getMax() < start[task]->getMax0()) {
            expl.push(start[task]->getMaxLit());
#ifdef VERBOSE_PROPAGATION_VERIFICATION
            std::cout << "UB_start, " << task << ", " << start[task]->getMax() << "; ";
#endif
        }
        if (dur[task]->getMin() > dur[task]->getMin0()) {
            // Lower bound of the duration variable matters
            expl.push(dur[task]->getMinLit());
#ifdef VERBOSE_PROPAGATION_VERIFICATION
            std::cout << "LB_p, " << task << ", " << dur[task]->getMin() << "; ";
#endif
        }
        if (usage[task]->getMin() > usage[task]->getMin0()) {
            // Lower bound of the usage variable matters
            expl.push(usage[task]->getMinLit());
#ifdef VERBOSE_PROPAGATION_VERIFICATION
            std::cout << "LB_c, " << task << ", " << usage[task]->getMin() << "; ";
#endif
        }
    }

    void lifted_explain_task(vec<Lit>& expl, int task, int lb, int ub) {
        if (lb > start[task]->getMin0()) {
            expl.push(getNegGeqLit(start[task], lb));
#ifdef VERBOSE_PROPAGATION_VERIFICATION
            std::cout << "LB_start, " << task << ", " << lb << "; ";
#endif
        }
        if (ub < start[task]->getMax0()) {
            expl.push(getNegLeqLit(start[task], ub));
#ifdef VERBOSE_PROPAGATION_VERIFICATION
            std::cout << "UB_start, " << task << ", " << ub << "; ";
#endif
        }
        if (dur[task]->getMin() > dur[task]->getMin0()) {
            // Lower bound of the duration variable matters
            expl.push(dur[task]->getMinLit());
#ifdef VERBOSE_PROPAGATION_VERIFICATION
            std::cout << "LB_p, " << task << ", " << dur[task]->getMin() << "; ";
#endif
        }
        if (usage[task]->getMin() > usage[task]->getMin0()) {
            // Lower bound of the usage variable matters
            expl.push(usage[task]->getMinLit());
#ifdef VERBOSE_PROPAGATION_VERIFICATION
            std::cout << "LB_c, " << task << ", " << usage[task]->getMin() << "; ";
#endif
        }
    }

    // Directly from the code of cumulative
    // Wrapper to get the negated literal -[[v <= val]] = [[v >= val + 1]]
    static Lit getNegLeqLit(IntVar* v, int val) {
        // return v->getLit(val + 1, LR_GE);
        return (INT_VAR_LL == v->getType() ? v->getMaxLit() : v->getLit(val + 1, LR_GE));
    }
    // Wrapper to get the negated literal -[[v >= val]] = [[ v <= val - 1]]
    static Lit getNegGeqLit(IntVar* v, int val) {
        // return v->getLit(val - 1, LR_LE);
        return (INT_VAR_LL == v->getType() ? v->getMinLit() : v->getLit(val - 1, LR_LE));
    }

    static void submit_conflict_explanation(vec<Lit>& expl) {
        Clause * reason = nullptr;
        if (so.lazy) {
            reason = Reason_new(expl.size());
            int i = 0;
            for (; i < expl.size(); i++) { (*reason)[i] = expl[i]; }
        }
        sat.confl = reason;
    }

    static Clause* get_reason_for_update(vec<Lit>& expl) {
        Clause* reason = Reason_new(expl.size() + 1);
        for (int i = 1; i <= expl.size(); i++) {
            (*reason)[i] = expl[i - 1];
        }
        return reason;
    }
};

void cumulative_kaoc(vec<IntVar*>& s, vec<IntVar*>& d, vec<IntVar*>& r, int limit, std::list<std::string>& opt) {
    // ASSUMPTION
    // - s, d, and r contain the same number of elements
    rassert(s.size() == d.size() && s.size() == r.size());
    vec<IntVar*> new_s;
    new_s.reserve(s.size());
    vec<IntVar*> new_d;
    new_d.reserve(s.size());
    vec<IntVar*> new_r;
    new_r.reserve(s.size());

    int r_sum = 0;
    for(int i = 0; i < s.size(); i++) {
        rassert(d[i]->getMin() >= 0 && r[i]->getMin() >= 0);
        if (d[i]->getMax() > 0 && r[i]->getMax() > 0) {
            new_s.push(s[i]);
            new_d.push(d[i]);
            new_r.push(r[i]);

            r_sum += std::min(r[i]->getMax(), limit);
            rassert(r_sum >= 0);
        }
    }
    if (r_sum <= limit) {
        return;
    }

    bool reason_with_holes = true;
    for (auto& it : opt) {
        if (it == "holes_on") {
            reason_with_holes = true;
        } else if (it == "holes_off") {
            reason_with_holes = false;
        }
    }
    if (reason_with_holes) {
        new CumulativeKAOC<true>(new_s, new_d, new_r, limit, opt);
    }
    else {
        new CumulativeKAOC<false>(new_s, new_d, new_r, limit, opt);
    }
}

#undef VERBOSE_PROPAGATION_VERIFICATION

