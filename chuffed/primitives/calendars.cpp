//
// Created by Samuel Cloutier on 2022-10-12.
//

#include <algorithm>
#include <chuffed/core/propagator.h>
#include <chuffed/support/calendar.h>

std::map<std::vector<int>, CalendarNoOver> CalendarFactory::memoized_calendars_no_over;
std::map<std::vector<int>, CalendarDay> CalendarFactory::memoized_calendars_day;
std::map<std::vector<int>, CalendarHour> CalendarFactory::memoized_calendars_hour;


class CalendarNoOverProp : public Propagator {
public:
    CalendarNoOverProp(IntVar* _var_start, IntVar* _var_elapsed_time, int _p, const CalendarNoOver* _cal) :
            var_start(_var_start), var_elapsed_time(_var_elapsed_time), p(_p), cal(_cal) {
        var_start->attach(this, 0, EVENT_LU);
        var_elapsed_time->attach(this, 1, EVENT_LU);
    }

    Clause* explain_update(const TaskDoms& new_doms, const TaskDoms& old_doms, int val) {
        assert(val >= 0 && val < 4);
        Clause* reason = nullptr;
        if (so.lazy) {
            vec<Lit> explanation;
            if (val != 1) {
                explanation.push(var_start->getMinLit());
            }
            if (val != 0) {
                explanation.push(var_start->getMaxLit());
            }
            if (val != 3) {
                explanation.push(var_elapsed_time->getMinLit());
            }
            if (val != 2) {
                explanation.push(var_elapsed_time->getMaxLit());
            }

            reason = Reason_new(explanation.size() + 1);
            for (int i = 1; i <= explanation.size(); i++) {
                (*reason)[i] = explanation[i - 1];
            }
        }
        return reason;
    }

    void explain_and_submit_conflict(const TaskDoms& old_doms) {
        Clause* reason = nullptr;
        if (so.lazy) {
            vec<Lit> explanation;
            explanation.push(var_start->getMinLit());
            explanation.push(var_start->getMaxLit());
            explanation.push(var_elapsed_time->getMinLit());
            explanation.push(var_elapsed_time->getMaxLit());

            reason = Reason_new(explanation.size());
            for (int i = 0; i < explanation.size(); i++) {
                (*reason)[i] = explanation[i];
            }
        }
        sat.confl = reason;
    }

    virtual bool filter_start(TaskDoms& new_doms, const TaskDoms& old_doms) {
        new_doms.min_S = cal->bound_start(new_doms, p, true);

        // Explanation of the conflict
        if (new_doms.min_S == INT_MAX) {
            explain_and_submit_conflict(old_doms);
            return false;
        }

        new_doms.max_S = cal->bound_start(new_doms, p, false);

        assert((new_doms.min_S == INT_MAX && new_doms.max_S == INT_MIN) || (new_doms.min_S != INT_MAX && new_doms.max_S != INT_MIN));

        // Explanation of the propagation on the lower bound of var_start
        if (new_doms.min_S != old_doms.min_S) {
            if (var_start->setMinNotR(new_doms.min_S) && !var_start->setMin(new_doms.min_S, explain_update(new_doms, old_doms, 0))) {
                return false;
            }
        }

        // Explanation of the propagation on the upper bound of var_start
        if (new_doms.max_S != old_doms.max_S) {
            if (var_start->setMaxNotR(new_doms.max_S) && !var_start->setMax(new_doms.max_S, explain_update(new_doms, old_doms, 1))) {
                return false;
            }
        }

        return true;
    }

    virtual bool filter_elapsed(TaskDoms& new_doms, const TaskDoms& old_doms) {
        new_doms.min_E = cal->bound_elapsed(new_doms, p, true);

        // Explanation of the conflict
        if (new_doms.min_E == INT_MAX) {
            explain_and_submit_conflict(old_doms);
            return false;
        }

        new_doms.max_E = cal->bound_elapsed(new_doms, p, false);

        assert((new_doms.min_E == INT_MAX && new_doms.max_E == INT_MIN) || (new_doms.min_E != INT_MAX && new_doms.max_E != INT_MIN));

        // Explanation of the propagation on the lower bound of var_elapsed
        if (new_doms.min_E != old_doms.min_E) {
            if (var_elapsed_time->setMinNotR(new_doms.min_E) && !var_elapsed_time->setMin(new_doms.min_E, explain_update(new_doms, old_doms, 2))) {
                return false;
            }
        }

        // Explanation of the propagation on the upper bound of var_elapsed
        if (new_doms.max_E != old_doms.max_E) {
            if (var_elapsed_time->setMaxNotR(new_doms.max_E) && !var_elapsed_time->setMax(new_doms.max_E, explain_update(new_doms, old_doms, 3))) {
                return false;
            }
        }

        return true;
    }

    bool propagate() override {
        TaskDoms old_doms = TaskDoms(var_start->getMin(), var_start->getMax(),
                                     var_elapsed_time->getMin(),var_elapsed_time->getMax());
        TaskDoms new_doms = TaskDoms(old_doms);

        // Un meilleur ordre existe-t-il ?
        if (!filter_start(new_doms, old_doms)) return false;
        if (!filter_elapsed(new_doms, old_doms)) return false;
        return true;
    }

protected:
    IntVar* var_start;
    IntVar* var_elapsed_time;
    int p;
    const CalendarDay* cal;
};


class CalendarDayProp : public Propagator {
public:
    CalendarDayProp(IntVar* _var_start, IntVar* _var_overtime, IntVar* _var_elapsed_time, int _p, const CalendarDay* _cal) :
            var_start(_var_start), var_overtime(_var_overtime), var_elapsed_time(_var_elapsed_time), p(_p), cal(_cal) {
        var_start->attach(this, 0, EVENT_LU);
        var_overtime->attach(this, 1, EVENT_LU);
        var_elapsed_time->attach(this, 2, EVENT_LU);
    }

    Clause* explain_update(const TaskDoms& new_doms, const TaskDoms& old_doms, int val) {
        assert(val >= 0 && val < 6);
        Clause* reason = nullptr;
        if (so.lazy) {
            vec<Lit> explanation;
            if (val != 1) {
                explanation.push(var_start->getMinLit());
            }
            if (val != 0) {
                explanation.push(var_start->getMaxLit());
            }
            if (val != 3) {
                explanation.push(var_elapsed_time->getMinLit());
            }
            if (val != 2) {
                explanation.push(var_elapsed_time->getMaxLit());
            }
            if (val != 5) {
                explanation.push(var_overtime->getMinLit());
            }
            if (val != 4) {
                explanation.push(var_overtime->getMaxLit());
            }

            reason = Reason_new(explanation.size() + 1);
            for (int i = 1; i <= explanation.size(); i++) {
                (*reason)[i] = explanation[i - 1];
            }
        }
        return reason;
    }

    void explain_and_submit_conflict(const TaskDoms& old_doms) {
        Clause* reason = nullptr;
        if (so.lazy) {
            vec<Lit> explanation;
            explanation.push(var_start->getMinLit());
            explanation.push(var_start->getMaxLit());
            explanation.push(var_elapsed_time->getMinLit());
            explanation.push(var_elapsed_time->getMaxLit());
            explanation.push(var_overtime->getMinLit());
            explanation.push(var_overtime->getMaxLit());

            reason = Reason_new(explanation.size());
            for (int i = 0; i < explanation.size(); i++) {
                (*reason)[i] = explanation[i];
            }
        }
        sat.confl = reason;
    }

    virtual bool filter_start(TaskDoms& new_doms, const TaskDoms& old_doms) {
        new_doms.min_S = cal->bound_start(new_doms, p, true);

        // Explanation of the conflict
        if (new_doms.min_S == INT_MAX) {
            explain_and_submit_conflict(old_doms);
            return false;
        }

        new_doms.max_S = cal->bound_start(new_doms, p, false);

        assert((new_doms.min_S == INT_MAX && new_doms.max_S == INT_MIN) || (new_doms.min_S != INT_MAX && new_doms.max_S != INT_MIN));

        // Explanation of the propagation on the lower bound of var_start
        if (new_doms.min_S != old_doms.min_S) {
            if (var_start->setMinNotR(new_doms.min_S) && !var_start->setMin(new_doms.min_S, explain_update(new_doms, old_doms, 0))) {
                return false;
            }
        }

        // Explanation of the propagation on the upper bound of var_start
        if (new_doms.max_S != old_doms.max_S) {
            if (var_start->setMaxNotR(new_doms.max_S) && !var_start->setMax(new_doms.max_S, explain_update(new_doms, old_doms, 1))) {
                return false;
            }
        }

        return true;
    }

    virtual bool filter_elapsed(TaskDoms& new_doms, const TaskDoms& old_doms) {
        new_doms.min_E = cal->bound_elapsed(new_doms, p, true);

        // Explanation of the conflict
        if (new_doms.min_E == INT_MAX) {
            explain_and_submit_conflict(old_doms);
            return false;
        }

        new_doms.max_E = cal->bound_elapsed(new_doms, p, false);

        assert((new_doms.min_E == INT_MAX && new_doms.max_E == INT_MIN) || (new_doms.min_E != INT_MAX && new_doms.max_E != INT_MIN));

        // Explanation of the propagation on the lower bound of var_elapsed
        if (new_doms.min_E != old_doms.min_E) {
            if (var_elapsed_time->setMinNotR(new_doms.min_E) && !var_elapsed_time->setMin(new_doms.min_E, explain_update(new_doms, old_doms, 2))) {
                return false;
            }
        }

        // Explanation of the propagation on the upper bound of var_elapsed
        if (new_doms.max_E != old_doms.max_E) {
            if (var_elapsed_time->setMaxNotR(new_doms.max_E) && !var_elapsed_time->setMax(new_doms.max_E, explain_update(new_doms, old_doms, 3))) {
                return false;
            }
        }

        return true;
    }

    virtual bool filter_over(TaskDoms& new_doms, const TaskDoms& old_doms) {
        new_doms.min_O = cal->bound_over(new_doms, p, true);

        // Explanation of the conflict
        if (new_doms.min_O == INT_MAX) {
            explain_and_submit_conflict(old_doms);
            return false;
        }

        new_doms.max_O = cal->bound_over(new_doms, p, false);

        assert((new_doms.min_O == INT_MAX && new_doms.max_O == INT_MIN) || (new_doms.min_O != INT_MAX && new_doms.max_O != INT_MIN));

        // Explanation of the propagation on the lower bound of var_overtime
        if (new_doms.min_O != old_doms.min_O) {
            if (var_overtime->setMinNotR(new_doms.min_O) && !var_overtime->setMin(new_doms.min_O, explain_update(new_doms, old_doms, 4))) {
                return false;
            }
        }

        // Explanation of the propagation on the upper bound of var_overtime
        if (new_doms.max_O != old_doms.max_O) {
            if (var_overtime->setMaxNotR(new_doms.max_O) && !var_overtime->setMax(new_doms.max_O, explain_update(new_doms, old_doms, 5))) {
                return false;
            }
        }

        return true;
    }

    bool propagate() override {
        TaskDoms old_doms = TaskDoms(var_start->getMin(), var_start->getMax(), var_elapsed_time->getMin(),
                           var_elapsed_time->getMax(), var_overtime->getMin(), var_overtime->getMax());
        TaskDoms new_doms = TaskDoms(old_doms);

        // Un meilleur ordre existe-t-il ?
        if (!filter_start(new_doms, old_doms)) return false;
        if (!filter_elapsed(new_doms, old_doms)) return false;
        if (!filter_over(new_doms, old_doms)) return false;
        return true;
    }

protected:
    IntVar* var_start;
    IntVar* var_overtime;
    IntVar* var_elapsed_time;
    int p;
    const CalendarDay* cal;
};


class CalendarHourProp : public Propagator {
public:
    CalendarHourProp(IntVar* _var_start, IntVar* _var_overtime, IntVar* _var_elapsed_time, int _p, const CalendarHour* _cal) :
            var_start(_var_start), var_overtime(_var_overtime), var_elapsed_time(_var_elapsed_time), p(_p), cal(_cal) {
        var_start->attach(this, 0, EVENT_LU);
        var_overtime->attach(this, 1, EVENT_LU);
        var_elapsed_time->attach(this, 2, EVENT_LU);
    }

    Clause* explain_update(const TaskDoms& new_doms, const TaskDoms& old_doms, int val) {
        assert(val >= 0 && val < 6);
        Clause* reason = nullptr;
        if (so.lazy) {
            vec<Lit> explanation;
            if (val != 1) {
                explanation.push(var_start->getMinLit());
            }
            if (val != 0) {
                explanation.push(var_start->getMaxLit());
            }
            if (val != 3) {
                explanation.push(var_elapsed_time->getMinLit());
            }
            if (val != 2) {
                explanation.push(var_elapsed_time->getMaxLit());
            }
            if (val != 5) {
                explanation.push(var_overtime->getMinLit());
            }
            if (val != 4) {
                explanation.push(var_overtime->getMaxLit());
            }

            reason = Reason_new(explanation.size() + 1);
            for (int i = 1; i <= explanation.size(); i++) {
                (*reason)[i] = explanation[i - 1];
            }
        }
        return reason;
    }

    void explain_and_submit_conflict(const TaskDoms& old_doms) {
        Clause* reason = nullptr;
        if (so.lazy) {
            vec<Lit> explanation;
            explanation.push(var_start->getMinLit());
            explanation.push(var_start->getMaxLit());
            explanation.push(var_elapsed_time->getMinLit());
            explanation.push(var_elapsed_time->getMaxLit());
            explanation.push(var_overtime->getMinLit());
            explanation.push(var_overtime->getMaxLit());

            reason = Reason_new(explanation.size());
            for (int i = 0; i < explanation.size(); i++) {
                (*reason)[i] = explanation[i];
            }
        }
        sat.confl = reason;
    }

    virtual bool filter_start(TaskDoms& new_doms, const TaskDoms& old_doms) {
        new_doms.min_S = cal->bound_start(new_doms, p, true);

        // Explanation of the conflict
        if (new_doms.min_S == INT_MAX) {
            explain_and_submit_conflict(old_doms);
            return false;
        }

        new_doms.max_S = cal->bound_start(new_doms, p, false);

        assert((new_doms.min_S == INT_MAX && new_doms.max_S == INT_MIN) || (new_doms.min_S != INT_MAX && new_doms.max_S != INT_MIN));

        // Explanation of the propagation on the lower bound of var_start
        if (new_doms.min_S != old_doms.min_S) {
            if (var_start->setMinNotR(new_doms.min_S) && !var_start->setMin(new_doms.min_S, explain_update(new_doms, old_doms, 0))) {
                return false;
            }
        }

        // Explanation of the propagation on the upper bound of var_start
        if (new_doms.max_S != old_doms.max_S) {
            if (var_start->setMaxNotR(new_doms.max_S) && !var_start->setMax(new_doms.max_S, explain_update(new_doms, old_doms, 1))) {
                return false;
            }
        }

        return true;
    }

    virtual bool filter_elapsed(TaskDoms& new_doms, const TaskDoms& old_doms) {
        new_doms.min_E = cal->bound_elapsed(new_doms, p, true);

        // Explanation of the conflict
        if (new_doms.min_E == INT_MAX) {
            explain_and_submit_conflict(old_doms);
            return false;
        }

        new_doms.max_E = cal->bound_elapsed(new_doms, p, false);

        assert((new_doms.min_E == INT_MAX && new_doms.max_E == INT_MIN) || (new_doms.min_E != INT_MAX && new_doms.max_E != INT_MIN));

        // Explanation of the propagation on the lower bound of var_elapsed
        if (new_doms.min_E != old_doms.min_E) {
            if (var_elapsed_time->setMinNotR(new_doms.min_E) && !var_elapsed_time->setMin(new_doms.min_E, explain_update(new_doms, old_doms, 2))) {
                return false;
            }
        }

        // Explanation of the propagation on the upper bound of var_elapsed
        if (new_doms.max_E != old_doms.max_E) {
            if (var_elapsed_time->setMaxNotR(new_doms.max_E) && !var_elapsed_time->setMax(new_doms.max_E, explain_update(new_doms, old_doms, 3))) {
                return false;
            }
        }

        return true;
    }

    virtual bool filter_over(TaskDoms& new_doms, const TaskDoms& old_doms) {
        new_doms.min_O = cal->bound_over(new_doms, p, true);

        // Explanation of the conflict
        if (new_doms.min_O == INT_MAX) {
            explain_and_submit_conflict(old_doms);
            return false;
        }

        new_doms.max_O = cal->bound_over(new_doms, p, false);

        assert((new_doms.min_O == INT_MAX && new_doms.max_O == INT_MIN) || (new_doms.min_O != INT_MAX && new_doms.max_O != INT_MIN));

        // Explanation of the propagation on the lower bound of var_overtime
        if (new_doms.min_O != old_doms.min_O) {
            if (var_overtime->setMinNotR(new_doms.min_O) && !var_overtime->setMin(new_doms.min_O, explain_update(new_doms, old_doms, 4))) {
                return false;
            }
        }

        // Explanation of the propagation on the upper bound of var_overtime
        if (new_doms.max_O != old_doms.max_O) {
            if (var_overtime->setMaxNotR(new_doms.max_O) && !var_overtime->setMax(new_doms.max_O, explain_update(new_doms, old_doms, 5))) {
                return false;
            }
        }

        return true;
    }

    bool propagate() override {
        TaskDoms old_doms = TaskDoms(var_start->getMin(), var_start->getMax(), var_elapsed_time->getMin(),
                           var_elapsed_time->getMax(), var_overtime->getMin(), var_overtime->getMax());
        TaskDoms new_doms = TaskDoms(old_doms);

        // Un meilleur ordre existe-t-il ?
        if (!filter_start(new_doms, old_doms)) return false;
        if (!filter_elapsed(new_doms, old_doms)) return false;
        if (!filter_over(new_doms, old_doms)) return false;
        return true;
    }

protected:
    IntVar* var_start;
    IntVar* var_overtime;
    IntVar* var_elapsed_time;
    int p;
    const CalendarHour* cal;
};


void calendar_day(IntVar* start, IntVar* overtime, IntVar* elapsed_time, int p_i, vec<int>& calendar) {
    rassert(p_i >= 0);
    rassert(p_i < calendar.size());
    rassert(start->getMin() >= 0);
    rassert(start->getMax() < calendar.size());
    rassert(overtime->getMin() >= 0);
    rassert(overtime->getMax() <= p_i);
    rassert(elapsed_time->getMin() >= 0);
    rassert(elapsed_time->getMax() <= calendar.size());

    std::vector<int> calendar_vector;
    calendar_vector.reserve(calendar.size());
    for (int i = 0; i < calendar.size(); i++) {
        rassert(calendar[i] >= 0);
        rassert(calendar[i] <= 1);
        calendar_vector.push_back(calendar[i]);
    }

//    if (overtime->getMax() == 0) {
//        new CalendarNoOverProp(start, elapsed_time, p_i, &CalendarFactory::get_calendar_no_over(calendar_vector));
//    }
//    else {
        new CalendarDayProp(start, overtime, elapsed_time, p_i, &CalendarFactory::get_calendar_day(calendar_vector));
//    }
}

void calendar_hour(IntVar* start, IntVar* overtime, IntVar* elapsed_time, int p_i, vec<int>& calendar) {
    rassert(p_i >= 0);
    rassert(p_i < calendar.size());
    rassert(start->getMin() >= 0);
    rassert(start->getMax() < calendar.size());
    rassert(overtime->getMin() >= 0);
    rassert(overtime->getMax() <= p_i);
    rassert(elapsed_time->getMin() >= 0);
    rassert(elapsed_time->getMax() <= calendar.size());

//    bool no_over = true;

    std::vector<int> calendar_vector;
    calendar_vector.reserve(calendar.size());
    for (int i = 0; i < calendar.size(); i++) {
        rassert(calendar[i] >= 0);
        rassert(calendar[i] <= 2);
//        if (no_over && calendar[i] == 2) {
//            no_over = false;
//        }
        calendar_vector.push_back(calendar[i]);
    }
//    if (no_over || overtime->getMax() == 0) {
//        if (overtime->getMax() != 0) {
//            int_rel(overtime, IRT_EQ, 0);
//        }
//        new CalendarNoOverProp(start, elapsed_time, p_i, &CalendarFactory::get_calendar_no_over(calendar_vector));
//    }
//    else {
        new CalendarHourProp(start, overtime, elapsed_time, p_i, &CalendarFactory::get_calendar_hour(calendar_vector));
//    }
}
