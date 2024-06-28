//
// Created by Samuel Cloutier on 2023-02-06.
//

#include <chuffed/support/calendar.h>


//======================================================================================================================
// Class CalendarDay
//======================================================================================================================

CalendarDay::CalendarDay(const std::vector<int>& cal) {
    I.reserve(int(cal.size()));

    I.push((cal[0] == 1) - 1);
    if (cal[0] == 1) X.push(0);

    for (int i = 1; i < cal.size(); i++) {
        I.push(I[i - 1] + (cal[i] == 1));
        if (cal[i] == 1) X.push(i);
    }
}

bool CalendarDay::workable(int time, time_type type) const {
    bool workable;
    if (time < 0 || time >= I.size()) return false;

    if (time == 0) {
        workable = I[0] + 1;
    }
    else {
        workable = I[time] - I[time - 1];
    }

    return workable;
}

int CalendarDay::next_workable(int time, time_type type) const {
    int next;

    if (time < 0) time = 0;
    if (workable(time, type)) {
        next = time;
    }
    else {
        if (time >= I.size() || I[time] + 1 >= X.size()) return I.size() + 1;
        next = X[I[time] + 1];
    }

    return next;
}

int CalendarDay::previous_workable(int time, time_type type) const {
    int previous;

    if (time >= I.size()) time = I.size() - 1;
    if (workable(time, type)) {
        previous = time;
    }
    else {
        if (time < 0 || I[time] < 0) return -1;
        previous = X[I[time]];
    }

    return previous;
}

int CalendarDay::count_time(int begin, int end, time_type type) const {
    // counts days in the interval [begin, end)

    if (end > I.size()) end = I.size();
    if (begin >= end || end < 1) return 0;

    int strictly_before_begin;
    if (begin < 1) strictly_before_begin = -1;
    else strictly_before_begin = I[begin - 1];

    return I[end - 1] - strictly_before_begin;
}

int CalendarDay::get_end(int start, int working_time, time_type type) const {
    int end;

    if (working_time <= 0) return start;

    if (start < 0) start = 0;
    if (start >= I.size()) return I.size() + 1;

    if (I[start] + (working_time - workable(start, type)) >= X.size()) end = I.size() + 1;
    else end = X[I[start] + (working_time - workable(start, type))] + 1;

    return end;
}

int CalendarDay::get_start(int end, int working_time, time_type type) const {
    assert(type != overtime_hours);
    int start;

    if (working_time <= 0) return end;

    if (end > I.size()) end = I.size();
    if (end < 1) return -1;

    if (I[end - 1] - (working_time - 1) < 0) start = -1;
    else start = X[I[end - 1] - (working_time - 1)];

    return start;
}

int CalendarDay::bound_start(const TaskDoms& doms, int p, bool min) const {
    assert(doms.min_S >= 0);
    assert(doms.max_S <= size());
    int ret_val = (min) ? INT_MAX : INT_MIN;
    int end;

    if (p < doms.min_O || doms.max_S < doms.min_S || doms.max_E < doms.min_E || doms.max_O < doms.min_O) return ret_val;

    if (min) {
        for (int start = next_workable(doms.min_S, any_time); start <= doms.max_S; start = next_workable(start + 1, any_time)) {
            // Smallest ending time that verifies min(E) and max(O).
            end = get_min_end(start, doms.min_E, doms.max_O, p);

            // If we get out of the calendar, it is clear that we are too far.
            // Since ending times are excluded from execution window, it is > and not >=.
            if (end > size()) {
                break;
            }

            // The ending time cannot be made smaller.
            // Thus, constraints on max(E) and min(O) cannot be made right if they are not verified now.
            if (end - start <= doms.max_E && p - count_time(start, end, any_time) >= doms.min_O) {
                ret_val = start;
                break;
            }
        }
    }
    else {
        for (int start = previous_workable(doms.max_S, any_time); start >= doms.min_S; start = previous_workable(start - 1, any_time)) {
            // Smallest ending time that verifies min(E) and max(O).
            end = get_min_end(start, doms.min_E, doms.max_O, p);

            // If we get out of the calendar, it is clear that we are too far.
            // Since ending times are excluded from execution window, it is > and not >=.
            if (end > size()) {
                continue;
            }

            // The ending time cannot be made smaller.
            // Thus, constraints on max(E) and min(O) cannot be made right if they are not verified now.
            if (end - start <= doms.max_E && p - count_time(start, end, any_time) >= doms.min_O) {
                ret_val = start;
                break;
            }
        }
    }

    return ret_val;
}

//int CalendarDay::bound_start(const TaskDoms& doms, int p, bool min) const {
//    assert(doms.min_S >= 0);
//    assert(doms.max_S <= size());
//    int ret_val = (min) ? INT_MAX : INT_MIN;
//    int end;
//
//    if (p < doms.min_O || doms.max_S < doms.min_S || doms.max_E < doms.min_E || doms.max_O < doms.min_O) {
//        return ret_val;
//    }
//
//    int k = 1;
//
//    if (min) {
//        for (int start = next_workable(doms.min_S, any_time); start <= doms.max_S; start = next_workable(start + std::max(1, k), any_time)) {
//            // Smallest ending time that verifies min(E) and max(O).
//            end = get_min_end(start, doms.min_E, doms.max_O, p);
//
//            // If we get out of the calendar, it is clear that we are too far.
//            // Since ending times are excluded from execution window, it is > and not >=.
//            if (end > size()) {
//                break;
//            }
//
//            k = end - start - doms.max_E;
//
//            // The ending time cannot be made smaller.
//            // Thus, constraints on max(E) and min(O) cannot be made right if they are not verified now.
//            if (k <= 0 && p - count_time(start, end, any_time) >= doms.min_O) {
//                ret_val = start;
//                break;
//            }
//        }
//    }
//    else {
//        for (int start = previous_workable(doms.max_S, any_time); start >= doms.min_S; start = previous_workable(start - std::max(1, k), any_time)) {
//            // Biggest ending time that verifies max(E) and min(O) without getting out of the calendar.
//            // We do not know is enough time is worked.
//            end = get_max_end(start, doms.max_E, doms.min_O, p);
//
//            // The ending time cannot be made bigger.
//            // Thus, constraints on min(E) and max(O) cannot be made right if they are not verified now.
//            k = doms.min_E - (end - start);
//            if (k <= 0 && p - count_time(start, end, any_time) <= doms.max_O) {
//                ret_val = start;
//                break;
//            }
//        }
//    }
//
//    return ret_val;
//}

int CalendarDay::bound_elapsed(const TaskDoms& doms, int p, bool min) const {
    assert(doms.min_S >= 0);
    assert(doms.max_S <= size());
    int ret_val = (min) ? INT_MAX : INT_MIN;
    int end;
    int elapsed;

    if (p < doms.min_O || doms.max_S < doms.min_S || doms.max_E < doms.min_E || doms.max_O < doms.min_O) return ret_val;

    if (min) {
        for (int start = next_workable(doms.min_S, any_time); start <= doms.max_S; start = next_workable(start + 1, any_time)) {
            // Smallest ending time that verifies min(E) and max(O).
            end = get_min_end(start, doms.min_E, doms.max_O, p);

            // If we get out of the calendar, it is clear that we are too far.
            // Since ending times are excluded from execution window, it is > and not >=.
            if (end > size()) {
                break;
            }

            // The ending time cannot be made smaller.
            // Thus, constraints on max(E) and min(O) cannot be made right if they are not verified now.
            elapsed = end - start;
            if (elapsed <= doms.max_E && p - count_time(start, end, any_time) >= doms.min_O) {
                if (ret_val > elapsed) ret_val = elapsed;
                if (ret_val == doms.min_E) break;
            }
        }
    }
    else {
        for (int start = next_workable(doms.min_S, any_time); start <= doms.max_S; start = next_workable(start + 1, any_time)) {
            // Biggest ending time that verifies max(E) and min(O) without getting out of the calendar.
            // We do not know is enough time is worked.
            end = get_max_end(start, doms.max_E, doms.min_O, p);

            // The ending time cannot be made bigger.
            // Thus, constraints on min(E) and max(O) cannot be made right if they are not verified now.
            elapsed = end - start;
            if (elapsed >= doms.min_E && p - count_time(start, end, any_time) <= doms.max_O) {
                if (ret_val < elapsed) ret_val = elapsed;
                if (ret_val == doms.max_E) break;
            }
        }
    }

    return ret_val;
}

//int CalendarDay::bound_elapsed(const TaskDoms& doms, int p, bool min) const {
//    assert(doms.min_S >= 0);
//    assert(doms.max_S <= size());
//    int ret_val = (min) ? INT_MAX : INT_MIN;
//    int end;
//    int elapsed;
//
//    if (p < doms.min_O || doms.max_S < doms.min_S || doms.max_E < doms.min_E || doms.max_O < doms.min_O) {
//        return ret_val;
//    }
//
//    int k = 1;
//
//    if (min) {
//        for (int start = next_workable(doms.min_S, any_time); start <= doms.max_S; start = next_workable(start + std::max(1, k), any_time)) {
//            // Smallest ending time that verifies min(E) and max(O).
//            end = get_min_end(start, doms.min_E, doms.max_O, p);
//
//            // If we get out of the calendar, it is clear that we are too far.
//            // Since ending times are excluded from execution window, it is > and not >=.
//            if (end > size()) {
//                break;
//            }
//
//            // The ending time cannot be made smaller.
//            // Thus, constraints on max(E) and min(O) cannot be made right if they are not verified now.
//            elapsed = end - start;
//            k = elapsed - doms.max_E;
//            if (k <= 0 && p - count_time(start, end, any_time) >= doms.min_O) {
//                if (ret_val > elapsed) {
//                    ret_val = elapsed;
//                }
//                if (ret_val == doms.min_E) {
//                    break;
//                }
//            }
//        }
//    }
//    else {
//        for (int start = previous_workable(doms.max_S, any_time); start >= doms.min_S; start = previous_workable(start - std::max(1, k), any_time)) {
//            // Biggest ending time that verifies max(E) and min(O) without getting out of the calendar.
//            // We do not know is enough time is worked.
//            end = get_max_end(start, doms.max_E, doms.min_O, p);
//
//            // The ending time cannot be made bigger.
//            // Thus, constraints on min(E) and max(O) cannot be made right if they are not verified now.
//            elapsed = end - start;
//            k = doms.min_E - elapsed;
//            if (k <= 0 && p - count_time(start, end, any_time) <= doms.max_O) {
//                if (ret_val < elapsed) {
//                    ret_val = elapsed;
//                }
//                if (ret_val == doms.max_E) {
//                    break;
//                }
//            }
//        }
//    }
//
//    return ret_val;
//}

int CalendarDay::bound_over(const TaskDoms& doms, int p, bool min) const {
    assert(doms.min_S >= 0);
    assert(doms.max_S <= size());
    int ret_val = (min) ? INT_MAX : INT_MIN;
    int end;
    int over;

    if (p < doms.min_O || doms.max_S < doms.min_S || doms.max_E < doms.min_E || doms.max_O < doms.min_O) return ret_val;

    if (min) {
        for (int start = next_workable(doms.min_S, any_time); start <= doms.max_S; start = next_workable(start + 1, any_time)) {
            // Biggest ending time that verifies max(E) and min(O) without getting out of the calendar.
            // We do not know is enough time is worked.
            end = get_max_end(start, doms.max_E, doms.min_O, p);

            // The ending time cannot be made bigger.
            // Thus, constraints on min(E) and max(O) cannot be made right if they are not verified now.
            over = p - count_time(start, end, any_time);
            if (end - start >= doms.min_E && over <= doms.max_O) {
                if (ret_val > over) ret_val = over;
                if (ret_val == doms.min_O) break;
            }
        }
    }
    else {
        for (int start = next_workable(doms.min_S, any_time); start <= doms.max_S; start = next_workable(start + 1, any_time)) {
            // Smallest ending time that verifies min(E) and max(O).
            end = get_min_end(start, doms.min_E, doms.max_O, p);

            // If we get out of the calendar, it is clear that we are too far.
            // Since ending times are excluded from execution window, it is > and not >=.
            if (end > size()) {
                break;
            }

            // The ending time cannot be made smaller.
            // Thus, constraints on max(E) and min(O) cannot be made right if they are not verified now.
            over = p - count_time(start, end, any_time);
            if (end - start <= doms.max_E && over >= doms.min_O) {
                if (ret_val < over) ret_val = over;
                if (ret_val == doms.max_O) break;
            }
        }
    }

    return ret_val;
}

//int CalendarDay::bound_over(const TaskDoms& doms, int p, bool min) const {
//    assert(doms.min_S >= 0);
//    assert(doms.max_S <= size());
//    int ret_val = (min) ? INT_MAX : INT_MIN;
//    int end;
//    int over;
//
//    if (p < doms.min_O || doms.max_S < doms.min_S || doms.max_E < doms.min_E || doms.max_O < doms.min_O) {
//        return ret_val;
//    }
//
//    int k = 1;
//
//    if (min) {
//        for (int start = previous_workable(doms.max_S, any_time); start >= doms.min_S; start = previous_workable(start - std::max(1, k), any_time)) {
//            // Biggest ending time that verifies max(E) and min(O) without getting out of the calendar.
//            // We do not know is enough time is worked.
//            end = get_max_end(start, doms.max_E, doms.min_O, p);
//
//            // The ending time cannot be made bigger.
//            // Thus, constraints on min(E) and max(O) cannot be made right if they are not verified now.
//            over = p - count_time(start, end, any_time);
//            k = doms.min_E - (end - start);
//            if (k <= 0 && over <= doms.max_O) {
//                if (ret_val > over) {
//                    ret_val = over;
//                }
//                if (ret_val == doms.min_O) {
//                    break;
//                }
//            }
//        }
//    }
//    else {
//        for (int start = next_workable(doms.min_S, any_time); start <= doms.max_S; start = next_workable(start + std::max(1, k), any_time)) {
//            // Smallest ending time that verifies min(E) and max(O).
//            end = get_min_end(start, doms.min_E, doms.max_O, p);
//
//            // If we get out of the calendar, it is clear that we are too far.
//            // Since ending times are excluded from execution window, it is > and not >=.
//            if (end > size()) {
//                break;
//            }
//
//            // The ending time cannot be made smaller.
//            // Thus, constraints on max(E) and min(O) cannot be made right if they are not verified now.
//            over = p - count_time(start, end, any_time);
//            k = end - start - doms.max_E;
//            if (k <= 0 && over >= doms.min_O) {
//                if (ret_val < over) {
//                    ret_val = over;
//                }
//                if (ret_val == doms.max_O) {
//                    break;
//                }
//            }
//        }
//    }
//
//    return ret_val;
//}

int CalendarDay::lst(int lct, TaskDoms& doms, int p) const {
    assert(doms.min_S >= 0);
    assert(doms.max_S <= size());
    int ret_val = INT_MIN;
//    int rough_lst = get_start(lct, p - doms.max_O, any_time);
    int rough_lst = lct;
    int end;
    int elapsed;

    for (int start = previous_workable(std::min(rough_lst, doms.max_S), any_time); start >= doms.min_S; start = previous_workable(start - 1, any_time)) {
        // Smallest ending time that verifies min(E) and max(O).
        end = get_min_end(start, doms.min_E, doms.max_O, p);

        // If we get out of the calendar or go beyond the lct, it is clear that we are too far.
        // Since ending times are excluded from execution window, it is > and not >=.
        if (end > size() || end > lct) {
            continue;
        }

        // The ending time cannot be made smaller.
        // Thus, constraints on max(E) and min(O) cannot be made right if they are not verified now.
        if (end - start <= doms.max_E && p - count_time(start, end, any_time) >= doms.min_O) {
            ret_val = start;
            break;
        }
    }
    return ret_val;
}

int CalendarDay::ect(int est, TaskDoms& doms, int p) const {
    assert(doms.min_S >= 0);
    assert(doms.max_S <= size());
    int ret_val = INT_MAX;
    int end;
    int elapsed;

    for (int start = next_workable(std::max(est, doms.min_S), any_time); start <= doms.max_S; start = next_workable(start + 1, any_time)) {
        // Smallest ending time that verifies min(E) and max(O).
        end = get_min_end(start, doms.min_E, doms.max_O, p);

        // If we get out of the calendar, it is clear that we are too far.
        // Since ending times are excluded from execution window, it is > and not >=.
        if (end > size()) {
            break;
        }

        // The ending time cannot be made smaller.
        // Thus, constraints on max(E) and min(O) cannot be made right if they are not verified now.
        if (end - start <= doms.max_E && p - count_time(start, end, any_time) >= doms.min_O) {
            ret_val = end;
            break;
        }
    }

    return ret_val;
}

//int CalendarDay::ect(int est, TaskDoms& doms, int p) const {
//    assert(doms.min_S >= 0);
//    assert(doms.max_S <= size());
//    int ret_val = INT_MAX;
//    int end;
//    int elapsed;
//
//    int k = 1;
//
//    for (int start = next_workable(std::max(est, doms.min_S), any_time); start <= doms.max_S; start = next_workable(start + std::max(1, k), any_time)) {
//        // Smallest ending time that verifies min(E) and max(O).
//        end = get_min_end(start, doms.min_E, doms.max_O, p);
//
//        // If we get out of the calendar, it is clear that we are too far.
//        // Since ending times are excluded from execution window, it is > and not >=.
//        if (end > size()) {
//            break;
//        }
//
//        k = end - start - doms.max_E;
//
//        // The ending time cannot be made smaller.
//        // Thus, constraints on max(E) and min(O) cannot be made right if they are not verified now.
//        if (k <= 0 && p - count_time(start, end, any_time) >= doms.min_O) {
//            ret_val = end;
//            break;
//        }
//    }
//
//    return ret_val;
//}

int CalendarDay::get_min_end(int start, int min_E, int max_O, int p) const {
    // Smallest ending time that verifies min(E).
    int end = next_workable(start + min_E - 1, any_time) + 1;

    int worked_days = count_time(start, end, any_time);
    int min_worked_days = p - max_O;
    // Smallest ending time that verifies min(E) and max(O).
    if (worked_days < min_worked_days) {
        end = get_end(start, min_worked_days, any_time);
    }
    return end;
}

int CalendarDay::get_max_end(int start, int max_E, int min_O, int p) const {
    // We make sure that we cannot get out of the calendar.
    int max_end = size();
    if (max_end > start + max_E) {
        max_end = start + max_E;
    }
    // Biggest ending time that verifies max(E) without getting out of the calendar.
    int end = previous_workable(max_end - 1, any_time) + 1;

    int worked_days = count_time(start, end, any_time);
    int max_worked_days = p - min_O;
    // Biggest ending time that verifies max(E) and min(O) without getting out of the calendar.
    if (worked_days > max_worked_days) {
        end = get_end(start, max_worked_days, any_time);
    }

    // We do not know is enough time is worked. The caller will
    return end;
}


//======================================================================================================================
// Class CalendarHour
//======================================================================================================================

CalendarHour::CalendarHour(const std::vector<int>& cal) {
    I_reg.reserve(int(cal.size()));
    I_over.reserve(int(cal.size()));
    I_all.reserve(int(cal.size()));

    I_reg.push((cal[0] == 1) - 1);
    I_over.push((cal[0] == 2) - 1);
    I_all.push((cal[0] >= 1) - 1);
    if (cal[0] == 1) X_reg.push(0);
    if (cal[0] == 2) X_over.push(0);
    if (cal[0] >= 1) X_all.push(0);

    for (int i = 1; i < cal.size(); i++) {
        I_reg.push(I_reg[i - 1] + (cal[i] == 1));
        I_over.push(I_over[i - 1] + (cal[i] == 2));
        I_all.push(I_all[i - 1] + (cal[i] >= 1));
        if (cal[i] == 1) X_reg.push(i);
        if (cal[i] == 2) X_over.push(i);
        if (cal[i] >= 1) X_all.push(i);
    }
}

bool CalendarHour::workable(int time, time_type type) const {
    bool workable;
    if (time < 0 || time >= I_reg.size()) return false;

    const vec<int>& I = (type == regular_hours) ? I_reg : ((type == overtime_hours) ? I_over : I_all);

    if (time == 0) {
    workable = I[0] + 1;
    }
    else {
    workable = I[time] - I[time - 1];
    }

    return workable;
}

int CalendarHour::next_workable(int time, time_type type) const {
    int next;

    if (time < 0) time = 0;
    if (workable(time, type)) {
    next = time;
    }
    else {
    const vec<int>& I = (type == regular_hours) ? I_reg : ((type == overtime_hours) ? I_over : I_all);
    const vec<int>& X = (type == regular_hours) ? X_reg : ((type == overtime_hours) ? X_over : X_all);

    if (time >= I.size() || I[time] + 1 >= X.size()) return I.size() + 1;
    next = X[I[time] + 1];
    }

    return next;
}

int CalendarHour::previous_workable(int time, time_type type) const {
    int previous;

    if (time >= I_reg.size()) time = I_reg.size() - 1;
    if (workable(time, type)) {
    previous = time;
    }
    else {
    const vec<int>& I = (type == regular_hours) ? I_reg : ((type == overtime_hours) ? I_over : I_all);
    const vec<int>& X = (type == regular_hours) ? X_reg : ((type == overtime_hours) ? X_over : X_all);

    if (time < 0 || I[time] < 0) return -1;
    previous = X[I[time]];
    }

    return previous;
}

int CalendarHour::count_time(int begin, int end, time_type type) const {
    // counts hours in the interval [begin, end) of the type 'type'

if (end > I_reg.size()) end = I_reg.size();
if (begin >= end || end < 1) return 0;

const vec<int>& I = (type == regular_hours) ? I_reg : ((type == overtime_hours) ? I_over : I_all);
const vec<int>& X = (type == regular_hours) ? X_reg : ((type == overtime_hours) ? X_over : X_all);

int strictly_before_begin;
if (begin < 1) strictly_before_begin = -1;
else strictly_before_begin = I[begin - 1];

return I[end - 1] - strictly_before_begin;
}

int CalendarHour::get_end(int begin, int working_time, time_type type) const {
    int end;
    const vec<int>& I = (type == regular_hours) ? I_reg : ((type == overtime_hours) ? I_over : I_all);
    const vec<int>& X = (type == regular_hours) ? X_reg : ((type == overtime_hours) ? X_over : X_all);

    if (working_time <= 0) return begin;

    if (begin < 0) begin = 0;
    if (begin >= I.size()) return I.size() + 1;

    if (I[begin] + (working_time - workable(begin, type)) >= X.size()) end = I.size() + 1;
    else end = X[I[begin] + (working_time - workable(begin, type))] + 1;

    return end;
}

int CalendarHour::get_start(int end, int working_time, time_type type) const {
    int start;
    const vec<int>& I = (type == regular_hours) ? I_reg : ((type == overtime_hours) ? I_over : I_all);
    const vec<int>& X = (type == regular_hours) ? X_reg : ((type == overtime_hours) ? X_over : X_all);

    if (working_time <= 0) return end;

    if (end > I.size()) end = I.size();
    if (end < 1) return -1;

    if (I[end - 1] - (working_time - 1) < 0) start = -1;
    else start = X[I[end - 1] - (working_time - 1)];

    return start;
}

int CalendarHour::bound_start(const TaskDoms& doms, int p, bool min) const {
    assert(doms.min_S >= 0);
    assert(doms.max_S <= size());
    int ret_val = (min) ? INT_MAX : INT_MIN;
    int end;

    if (p < doms.min_O || doms.max_S < doms.min_S || doms.max_E < doms.min_E || doms.max_O < doms.min_O) return ret_val;

    if (min) {
        for (int start = next_workable(doms.min_S, any_time); start <= doms.max_S; start = next_workable(start + 1, any_time)) {
            // Smallest ending time that verifies min(E), max(O), minimal number of accessible hours and tail constraint.
            end = get_min_end(start, doms.min_E, doms.max_O, p);

            // If we get out of the calendar, it is clear that we are too far.
            // Since ending times are excluded from execution window, it is > and not >=.
            if (end > size()) {
                break;
            }

            // As explained in get_min_end, considering the other constraints tells whether there exists a support.
            if (end - start <= doms.max_E && p - count_time(start, end, regular_hours) >= doms.min_O
                && verify_head_and_tail(start, end, p)) {
                ret_val = start;
                break;
            }
        }
    }
    else {
        for (int start = previous_workable(doms.max_S, any_time); start >= doms.min_S; start = previous_workable(start - 1, any_time)) {
            // Smallest ending time that verifies min(E), max(O), minimal number of accessible hours and tail constraint.
            end = get_min_end(start, doms.min_E, doms.max_O, p);

            // If we get out of the calendar or go beyond the lct, it is clear that we are too far.
            // Since ending times are excluded from execution window, it is > and not >=.
            if (end > size()) {
                continue;
            }

            // As explained in get_min_end, considering the other constraints tells whether there exists a support.
            if (end - start <= doms.max_E && p - count_time(start, end, regular_hours) >= doms.min_O
                && verify_head_and_tail(start, end, p)) {
                ret_val = start;
                break;
            }
        }
    }

    return ret_val;
}

//int CalendarHour::bound_start(const TaskDoms& doms, int p, bool min) const {
//    assert(doms.min_S >= 0);
//    assert(doms.max_S <= size());
//    int ret_val = (min) ? INT_MAX : INT_MIN;
//    int end;
//
//    if (p < doms.min_O || doms.max_S < doms.min_S || doms.max_E < doms.min_E || doms.max_O < doms.min_O) {
//        return ret_val;
//    }
//
//    int k = 1;
//
//    if (min) {
//        for (int start = next_workable(doms.min_S, any_time); start <= doms.max_S; start = ((workable(start, regular_hours)) ? next_workable(start + std::max(1, k), any_time) : std::min(next_workable(start + 1, regular_hours), next_workable(start + std::max(1, k), overtime_hours)))) {
//            // Smallest ending time that verifies min(E), max(O), minimal number of accessible hours and tail constraint.
//            end = get_min_end(start, doms.min_E, doms.max_E, doms.max_O, p);
//
//            // If we get out of the calendar, it is clear that we are too far.
//            // Since ending times are excluded from execution window, it is > and not >=.
//            if (end > size()) {
//                break;
//            }
//
//            k = end - start - doms.max_E;
//
//            // As explained in get_min_end, considering the other constraints tells whether there exists a support.
//            if (k <= 0 && p - count_time(start, end, regular_hours) >= doms.min_O
//                && verify_head_and_tail(start, end, p)) {
//                ret_val = start;
//                break;
//            }
//        }
//    }
//    else {
//        for (int start = previous_workable(doms.max_S, any_time); start >= doms.min_S; start = previous_workable(start - std::max(1, k), any_time)) {
//            // Biggest ending time that verifies max(E) and the maximum number of regular hours given min(O) without
//            // getting out of the calendar.
//            end = get_max_end(start, doms.max_E, doms.min_O, p);
//
//            // We remove the tail if it causes a problem.
//            if (workable(end - 1, overtime_hours) && !verify_head_and_tail(start, end, p)) {
//                // If the tail is from the same bloc as the head, it cannot be removed completely, to not break tasks with p=1.
//                if (count_time(start, end, regular_hours) == 0) {
//                    end = start + 1;
//                }
//                else {
//                    end = previous_workable(end - 1, regular_hours) + 1;
//                }
//            }
//
//            int regular_time_worked = count_time(start, end, regular_hours);
//            int over = p - count_time(start, end, regular_hours);
//            k = doms.min_E - (end - start);
//            // As explained in get_max_end, considering the other constraints tells whether there exists a support.
//            if (doms.max_O >= over && count_time(start, end, overtime_hours) >= over && k <= 0
//                && verify_head_and_tail(start, end, p)) {
//                ret_val = start;
//                break;
//            }
//        }
//    }
//
//    return ret_val;
//}

int CalendarHour::bound_elapsed(const TaskDoms& doms, int p, bool min) const {
    assert(doms.min_S >= 0);
    assert(doms.max_S <= size());
    int ret_val = (min) ? INT_MAX : INT_MIN;
    int end;
    int elapsed;

    if (p < doms.min_O || doms.max_S < doms.min_S || doms.max_E < doms.min_E || doms.max_O < doms.min_O) return ret_val;

    if (min) {
        for (int start = next_workable(doms.min_S, any_time); start <= doms.max_S; start = next_workable(start + 1, any_time)) {
            // Smallest ending time that verifies min(E), max(O), minimal number of accessible hours and tail constraint.
            end = get_min_end(start, doms.min_E, doms.max_O, p);

            // If we get out of the calendar, it is clear that we are too far.
            // Since ending times are excluded from execution window, it is > and not >=.
            if (end > size()) {
                break;
            }

            // As explained in get_min_end, considering the other constraints tells whether there exists a support.
            elapsed = end - start;
            if (elapsed <= doms.max_E && p - count_time(start, end, regular_hours) >= doms.min_O
                && verify_head_and_tail(start, end, p)) {
                if (ret_val > elapsed) ret_val = elapsed;
                if (ret_val == doms.min_E) break;
            }
        }
    }
    else {
        for (int start = next_workable(doms.min_S, any_time); start <= doms.max_S; start = next_workable(start + 1, any_time)) {
            // Biggest ending time that verifies max(E) and the maximum number of regular hours given min(O) without
            // getting out of the calendar.
            end = get_max_end(start, doms.max_E, doms.min_O, p);

            // We remove the tail if it causes a problem.
            if (workable(end - 1, overtime_hours) && !verify_head_and_tail(start, end, p)) {
                // If the tail is from the same bloc as the head, it cannot be removed completely, to not break tasks with p=1.
                if (count_time(start, end, regular_hours) == 0) {
                    end = start + 1;
                }
                else {
                    end = previous_workable(end - 1, regular_hours) + 1;
                }
            }

            int regular_time_worked = count_time(start, end, regular_hours);
            int over = p - count_time(start, end, regular_hours);
            elapsed = end - start;
            // As explained in get_max_end, considering the other constraints tells whether there exists a support.
            if (doms.max_O >= over && count_time(start, end, overtime_hours) >= over && elapsed >= doms.min_E
                && verify_head_and_tail(start, end, p)) {
                if (ret_val < elapsed) ret_val = elapsed;
                if (ret_val == doms.max_E) break;
            }
        }
    }

    return ret_val;
}

//int CalendarHour::bound_elapsed(const TaskDoms& doms, int p, bool min) const {
//    assert(doms.min_S >= 0);
//    assert(doms.max_S <= size());
//    int ret_val = (min) ? INT_MAX : INT_MIN;
//    int end;
//    int elapsed;
//
//    if (p < doms.min_O || doms.max_S < doms.min_S || doms.max_E < doms.min_E || doms.max_O < doms.min_O) {
//        return ret_val;
//    }
//
//    int k = 1;
//
//    if (min) {
//        for (int start = next_workable(doms.min_S, any_time); start <= doms.max_S; start = ((workable(start, regular_hours)) ? next_workable(start + std::max(1, k), any_time) : std::min(next_workable(start + 1, regular_hours), next_workable(start + std::max(1, k), overtime_hours)))) {
//            // Smallest ending time that verifies min(E), max(O), minimal number of accessible hours and tail constraint.
//            end = get_min_end(start, doms.min_E, doms.max_E, doms.max_O, p);
//
//            // If we get out of the calendar, it is clear that we are too far.
//            // Since ending times are excluded from execution window, it is > and not >=.
//            if (end > size()) {
//                break;
//            }
//
//            // As explained in get_min_end, considering the other constraints tells whether there exists a support.
//            elapsed = end - start;
//            k = elapsed - doms.max_E;
//            if (k <= 0 && p - count_time(start, end, regular_hours) >= doms.min_O
//                && verify_head_and_tail(start, end, p)) {
//                if (ret_val > elapsed) {
//                    ret_val = elapsed;
//                }
//                if (ret_val == doms.min_E) {
//                    break;
//                }
//            }
//        }
//    }
//    else {
//        for (int start = previous_workable(doms.max_S, any_time); start >= doms.min_S; start = previous_workable(start - std::max(1, k), any_time)) {
//            // Biggest ending time that verifies max(E) and the maximum number of regular hours given min(O) without
//            // getting out of the calendar.
//            end = get_max_end(start, doms.max_E, doms.min_O, p);
//
//            // We remove the tail if it causes a problem.
//            if (workable(end - 1, overtime_hours) && !verify_head_and_tail(start, end, p)) {
//                // If the tail is from the same bloc as the head, it cannot be removed completely, to not break tasks with p=1.
//                if (count_time(start, end, regular_hours) == 0) {
//                    end = start + 1;
//                }
//                else {
//                    end = previous_workable(end - 1, regular_hours) + 1;
//                }
//            }
//
//            int regular_time_worked = count_time(start, end, regular_hours);
//            int over = p - count_time(start, end, regular_hours);
//            elapsed = end - start;
//            k = doms.min_E - elapsed;
//            // As explained in get_max_end, considering the other constraints tells whether there exists a support.
//            if (doms.max_O >= over && count_time(start, end, overtime_hours) >= over && k <= 0
//                && verify_head_and_tail(start, end, p)) {
//                if (ret_val < elapsed) {
//                    ret_val = elapsed;
//                }
//                if (ret_val == doms.max_E) {
//                    break;
//                }
//            }
//        }
//    }
//
//    return ret_val;
//}

int CalendarHour::bound_over(const TaskDoms& doms, int p, bool min) const {
    assert(doms.min_S >= 0);
    assert(doms.max_S <= size());
    int ret_val = (min) ? INT_MAX : INT_MIN;
    int end;
    int over;

    if (p < doms.min_O || doms.max_S < doms.min_S || doms.max_E < doms.min_E || doms.max_O < doms.min_O) return ret_val;

    if (min) {
        for (int start = next_workable(doms.min_S, any_time); start <= doms.max_S; start = next_workable(start + 1, any_time)) {
            // Biggest ending time that verifies max(E) and the maximum number of regular hours given min(O) without
            // getting out of the calendar.
            end = get_max_end(start, doms.max_E, doms.min_O, p);

            // We remove the tail if it causes a problem.
            if (workable(end - 1, overtime_hours) && !verify_head_and_tail(start, end, p)) {
                // If the tail is from the same bloc as the head, it cannot be removed completely, to not break tasks with p=1.
                if (count_time(start, end, regular_hours) == 0) {
                    end = start + 1;
                }
                else {
                    end = previous_workable(end - 1, regular_hours) + 1;
                }
            }

            int regular_time_worked = count_time(start, end, regular_hours);
            over = p - count_time(start, end, regular_hours);
            // As explained in get_max_end, considering the other constraints tells whether there exists a support.
            if (doms.max_O >= over && count_time(start, end, overtime_hours) >= over
                && end - start >= doms.min_E && verify_head_and_tail(start, end, p)) {
                if (ret_val > over) ret_val = over;
                if (ret_val == doms.min_O) break;
            }
        }
    }
    else {
        for (int start = next_workable(doms.min_S, any_time); start <= doms.max_S; start = next_workable(start + 1, any_time)) {
            // Smallest ending time that verifies min(E), max(O), minimal number of accessible hours and tail constraint.
            end = get_min_end(start, doms.min_E, doms.max_O, p);

            // If we get out of the calendar, it is clear that we are too far.
            // Since ending times are excluded from execution window, it is > and not >=.
            if (end > size()) {
                break;
            }

            // As explained in get_min_end, considering the other constraints tells whether there exists a support.
            over = p - count_time(start, end, regular_hours);
            if (end - start <= doms.max_E && over >= doms.min_O && verify_head_and_tail(start, end, p)) {
                if (ret_val < over) ret_val = over;
                if (ret_val == doms.max_O) break;
            }
        }
    }

    return ret_val;
}

//int CalendarHour::bound_over(const TaskDoms& doms, int p, bool min) const {
//    assert(doms.min_S >= 0);
//    assert(doms.max_S <= size());
//    int ret_val = (min) ? INT_MAX : INT_MIN;
//    int end;
//    int over;
//
//    if (p < doms.min_O || doms.max_S < doms.min_S || doms.max_E < doms.min_E || doms.max_O < doms.min_O) {
//        return ret_val;
//    }
//
//    int k = 1;
//
//    if (min) {
//        for (int start = previous_workable(doms.max_S, any_time); start >= doms.min_S; start = previous_workable(start - std::max(1, k), any_time)) {
//            // Biggest ending time that verifies max(E) and the maximum number of regular hours given min(O) without
//            // getting out of the calendar.
//            end = get_max_end(start, doms.max_E, doms.min_O, p);
//
//            // We remove the tail if it causes a problem.
//            if (workable(end - 1, overtime_hours) && !verify_head_and_tail(start, end, p)) {
//                // If the tail is from the same bloc as the head, it cannot be removed completely, to not break tasks with p=1.
//                if (count_time(start, end, regular_hours) == 0) {
//                    end = start + 1;
//                }
//                else {
//                    end = previous_workable(end - 1, regular_hours) + 1;
//                }
//            }
//
//            int regular_time_worked = count_time(start, end, regular_hours);
//            over = p - count_time(start, end, regular_hours);
//            k = doms.min_E - (end - start);
//            // As explained in get_max_end, considering the other constraints tells whether there exists a support.
//            if (doms.max_O >= over && count_time(start, end, overtime_hours) >= over
//                && k <= 0 && verify_head_and_tail(start, end, p)) {
//                if (ret_val > over) {
//                    ret_val = over;
//                }
//                if (ret_val == doms.min_O) {
//                    break;
//                }
//            }
//        }
//    }
//    else {
//        for (int start = next_workable(doms.min_S, any_time); start <= doms.max_S; start = ((workable(start, regular_hours)) ? next_workable(start + std::max(1, k), any_time) : std::min(next_workable(start + 1, regular_hours), next_workable(start + std::max(1, k), overtime_hours)))) {
//            // Smallest ending time that verifies min(E), max(O), minimal number of accessible hours and tail constraint.
//            end = get_min_end(start, doms.min_E, doms.max_E, doms.max_O, p);
//
//            // If we get out of the calendar, it is clear that we are too far.
//            // Since ending times are excluded from execution window, it is > and not >=.
//            if (end > size()) {
//                break;
//            }
//
//            // As explained in get_min_end, considering the other constraints tells whether there exists a support.
//            over = p - count_time(start, end, regular_hours);
//            k = end - start - doms.max_E;
//            if (k <= 0 && over >= doms.min_O && verify_head_and_tail(start, end, p)) {
//                if (ret_val < over) {
//                    ret_val = over;
//                }
//                if (ret_val == doms.max_O) {
//                    break;
//                }
//            }
//        }
//    }
//
//    return ret_val;
//}

int CalendarHour::lst(int lct, TaskDoms& doms, int p) const {
    assert(doms.min_S >= 0);
    assert(doms.max_S <= size());
    int ret_val = INT_MIN;
    int rough_lst = get_start(lct, p, any_time);
    int end;
    int elapsed;

    for (int start = previous_workable(std::min(rough_lst, doms.max_S), any_time); start >= doms.min_S; start = previous_workable(start - 1, any_time)) {
        // Smallest ending time that verifies min(E), max(O), minimal number of accessible hours and tail constraint.
        end = get_min_end(start, doms.min_E, doms.max_O, p);

        // If we get out of the calendar or go beyond the lct, it is clear that we are too far.
        // Since ending times are excluded from execution window, it is > and not >=.
        if (end > size() || end > lct) {
            continue;
        }

        // As explained in get_min_end, considering the other constraints tells whether there exists a support.
        if (end - start <= doms.max_E && p - count_time(start, end, regular_hours) >= doms.min_O
            && verify_head_and_tail(start, end, p)) {
            ret_val = start;
            break;
        }
    }

    return ret_val;
}

//int CalendarHour::lst(int lct, TaskDoms& doms, int p) const {
//    assert(doms.min_S >= 0);
//    assert(doms.max_S <= size());
//    int ret_val = INT_MIN;
//    int rough_lst = get_start(lct, p, any_time);
//    int end;
//    int elapsed;
//
//    for (int start = previous_workable(std::min(rough_lst, doms.max_S), any_time); start >= doms.min_S; start = previous_workable(start - 1, any_time)) {
//        // Smallest ending time that verifies min(E), max(O), minimal number of accessible hours and tail constraint.
//        end = get_min_end(start, doms.min_E, doms.max_E, doms.max_O, p);
//
//        // If we get out of the calendar or go beyond the lct, it is clear that we are too far.
//        // Since ending times are excluded from execution window, it is > and not >=.
//        if (end > size() || end > lct) {
//            continue;
//        }
//
//        // As explained in get_min_end, considering the other constraints tells whether there exists a support.
//        if (end - start <= doms.max_E && p - count_time(start, end, regular_hours) >= doms.min_O
//            && verify_head_and_tail(start, end, p)) {
//            ret_val = start;
//            break;
//        }
//    }
//
//    return ret_val;
//}

int CalendarHour::ect(int est, TaskDoms& doms, int p) const {
    assert(doms.min_S >= 0);
    assert(doms.max_S <= size());
    int ret_val = INT_MAX;
    int end;
    int elapsed;

    for (int start = next_workable(std::max(doms.min_S, est), any_time); start <= doms.max_S; start = next_workable(start + 1, any_time)) {
        // Smallest ending time that verifies min(E), max(O), minimal number of accessible hours and tail constraint.
        end = get_min_end(start, doms.min_E, doms.max_O, p);

        // If we get out of the calendar, it is clear that we are too far.
        // Since ending times are excluded from execution window, it is > and not >=.
        if (end > size()) {
            break;
        }

        // As explained in get_min_end, considering the other constraints tells whether there exists a support.
        if (end - start <= doms.max_E && p - count_time(start, end, regular_hours) >= doms.min_O
            && verify_head_and_tail(start, end, p)) {
            ret_val = end;
            break;
        }
    }

    return ret_val;
}

//int CalendarHour::ect(int est, TaskDoms& doms, int p) const {
//    assert(doms.min_S >= 0);
//    assert(doms.max_S <= size());
//    int ret_val = INT_MAX;
//    int end;
//    int elapsed;
//
//    int k = 1;
//
//    for (int start = next_workable(std::max(doms.min_S, est), any_time); start <= doms.max_S; start = ((workable(start, regular_hours)) ? next_workable(start + std::max(1, k), any_time) : std::min(next_workable(start + 1, regular_hours), next_workable(start + std::max(1, k), overtime_hours)))) {
//        // Smallest ending time that verifies min(E), max(O), minimal number of accessible hours and tail constraint.
//        end = get_min_end(start, doms.min_E, doms.max_E, doms.max_O, p);
//
//        // If we get out of the calendar, it is clear that we are too far.
//        // Since ending times are excluded from execution window, it is > and not >=.
//        if (end > size()) {
//            break;
//        }
//
//        k = end - start - doms.max_E;
//
//        // As explained in get_min_end, considering the other constraints tells whether there exists a support.
//        if (k <= 0 && p - count_time(start, end, regular_hours) >= doms.min_O
//            && verify_head_and_tail(start, end, p)) {
//            ret_val = end;
//            break;
//        }
//    }
//
//    return ret_val;
//}

bool CalendarHour::verify_head_and_tail(int start, int end, int p) const {
    int over = workable(start, overtime_hours);
    if (end > start + 1) {
        over += workable(end - 1, overtime_hours);
    }
    return  over <= p - count_time(start, end, regular_hours);
}

int CalendarHour::get_min_end(int start, int min_E, int max_O, int p) const {
    // Smallest ending time that verifies the minimal number of accessible hours.
    // Plus petit temps de fin respectant le nombre minimal d'heures accessibles
    int end = get_end(start, p, any_time);

    // Smallest ending time that verifies min(E), the minimal number of accessible hours.
    if (end < start + min_E) {
        end = next_workable(start + min_E - 1, any_time) + 1;
    }

    // Smallest ending time that verifies min(E), max(O) and minimal number of accessible hours.
    int worked_regular_hours = count_time(start, end, regular_hours);
    int min_regular_hours = p - max_O;
    if (worked_regular_hours < min_regular_hours) {
        end = get_end(start, min_regular_hours, regular_hours);
    }

    // It is possible that there is a tail problem that can be solved by adding a regular hour at the end.
    // Smallest ending time that verifies min(E), max(O), minimal number of accessible hours and tail constraint.
    if (workable(end - 1, overtime_hours) && !verify_head_and_tail(start, end, p)) {
        int new_end = next_workable(end - 1, regular_hours) + 1;
        if (new_end <= size()) {
            // Else, the tail problem cannot be solved without getting out of the calendar.
            end = new_end;
        }
    }

    // We do not know if we get out of the calendar or respect max(E), min(O), or the head constraint.

    // If we get out of the calendar, the starting time is invalid. Else, we have the right number of worked time.

    // If max(E) is not verified, the starting time is invalid because the ending time is minimal given a subset of the constraints.

    // If min(O) is not verified, there are either too many regular hours worked, which cannot fixed given that the ending
    // time is minimal, or there is not enough overtime hours in the execution window, which means there are too many
    // regular hours worked since we had the right number of worked time. Anyway, it means that the starting time is invalid.

    // If the head constraint is not verified, it means that max(O) is too small to work the head in overtime. By the
    // way the ending time is computed, this means that the starting time is invalid. Indeed, in that we have that max(O)=0.

    // Thus, this ending time is minimal and considering the other constraints tells whether there exists a support for
    // this starting time.

    return end;
}

//int CalendarHour::get_min_end(int start, int min_E, int max_E, int max_O, int p) const {
//    // Smallest ending time that verifies the minimal number of accessible hours.
//    // Plus petit temps de fin respectant le nombre minimal d'heures accessibles
//    int end = get_end(start, p, any_time);
//
//    // Smallest ending time that verifies min(E), the minimal number of accessible hours.
//    if (end < start + min_E) {
//        end = next_workable(start + min_E - 1, any_time) + 1;
//    }
//
//    // Smallest ending time that verifies min(E), max(O) and minimal number of accessible hours.
//    int worked_regular_hours = count_time(start, end, regular_hours);
//    int min_regular_hours = p - max_O;
//    if (worked_regular_hours < min_regular_hours) {
//        end = get_end(start, min_regular_hours, regular_hours);
//    }
//
//    // It is possible that there is a tail problem that can be solved by adding a regular hour at the end.
//    // Smallest ending time that verifies min(E), max(O), minimal number of accessible hours and tail constraint.
//    if (workable(end - 1, overtime_hours) && !verify_head_and_tail(start, end, p)) {
//        int new_end = next_workable(end - 1, regular_hours) + 1;
//        if (new_end <= size() and new_end - start <= max_E) {
//            // Else, the tail problem cannot be solved without getting out of the calendar.
//            end = new_end;
//        }
//    }
//
//    // We do not know if we get out of the calendar or respect max(E), min(O), or the head constraint.
//
//    // If we get out of the calendar, the starting time is invalid. Else, we have the right number of worked time.
//
//    // If max(E) is not verified, the starting time is invalid because the ending time is minimal given a subset of the constraints.
//
//    // If min(O) is not verified, there are either too many regular hours worked, which cannot fixed given that the ending
//    // time is minimal, or there is not enough overtime hours in the execution window, which means there are too many
//    // regular hours worked since we had the right number of worked time. Anyway, it means that the starting time is invalid.
//
//    // If the head constraint is not verified, it means that max(O) is too small to work the head in overtime. By the
//    // way the ending time is computed, this means that the starting time is invalid. Indeed, in that we have that max(O)=0.
//
//    // Thus, this ending time is minimal and considering the other constraints tells whether there exists a support for
//    // this starting time.
//
//    return end;
//}

int CalendarHour::get_max_end(int start, int max_E, int min_O, int p) const {
    // On va avoir que l'on vérifie qu'on ne sort pas du calendrier, mais l'on n'aura plus la garantie que l'on a
    // suffisamment de temps. On va par contre s'assurer que l'on ne dépasse pas le temps.

    // We make sure that we cannot get out of the calendar.
    int max_end = size();
    if (max_end > start + max_E) {
        max_end = start + max_E;
    }
    // Biggest ending time that verifies max(E) without getting out of the calendar.
    int end = previous_workable(max_end - 1, any_time) + 1;

    // Biggest ending time that verifies max(E) and the maximum number of regular hours given min(O) (counting the head)
    // without getting out of the calendar.
    int regular_worked = count_time(start, end, regular_hours);
    int max_regular_worked = p - min_O - (min_O == 0 && workable(start, overtime_hours));
    if (regular_worked > max_regular_worked) {
        end = get_end(start, max_regular_worked, regular_hours);
        // For the ending time to be maximal, we must add a tail that is as long as possible.

        // Since the ending time as just been reduced, we know that this tail is not too long.
        int tail_end = previous_workable(next_workable(end, regular_hours), overtime_hours) + 1;
        if (end < tail_end) {
            end = tail_end;
        }
    }

    // We do not know is the execution window has as many hours as needed, if there are enough overtime hours for min(O),
    // if we verify min(E) and max(O), or if we verify the head/tail constraints.

    // If we are still missing hours (regular or overtime), the starting time is invalid since the ending time is maximal.

    // If min(E) is not verified, the starting time is invalid since the ending time is maximal.

    // If max(O) is not verified, it actually means that we are missing regular hours, which means that the starting time
    // is invalid.

    // If there is a tail problem, the tail can either be removed, or the starting time is invalid. Then, if there is
    // still a head issue, it means that the starting time is an overtime hours and max(O)=0. Then, the starting time
    // is invalid.

    // Briefly, while working with this ending time, we will first have to find and solve head/tail problems. Then,
    // considering the other constraints will tell whether there exists a support for this starting time.

    return end;
}


//======================================================================================================================
// Class CalendarNoOver
//======================================================================================================================

CalendarNoOver::CalendarNoOver(const std::vector<int>& cal) : CalendarDay(cal) {}

int CalendarNoOver::bound_start(const TaskDoms& doms, int p, bool min) const {
    assert(doms.min_S >= 0);
    assert(doms.max_S <= size());
    int ret_val = (min) ? INT_MAX : INT_MIN;
    int end;
    int elapsed;

    if (doms.max_S < doms.min_S || doms.max_E < doms.min_E) return ret_val;

    if (min) {
        for (int start = next_workable(doms.min_S, any_time); start <= doms.max_S; start = next_workable(start + 1, any_time)) {
            end = get_end(start, p, any_time);

            // If we get out of the calendar, it is clear that we are too far.
            // Since ending times are excluded from execution window, it is > and not >=.
            if (end > size()) {
                break;
            }

            elapsed = end - start;
            if (elapsed >= doms.min_E && elapsed <= doms.max_E) {
                // We found that this start is bound consistent.
                ret_val = start;
                break;
            }
        }
    }
    else {
        for (int start = previous_workable(doms.max_S, any_time); start >= doms.min_S; start = previous_workable(start - 1, any_time)) {
            end = get_end(start, p, any_time);

            // If we get out of the calendar, it is clear that we are too far.
            // Since ending times are excluded from execution window, it is > and not >=.
            if (end > size()) {
                continue;
            }

            elapsed = end - start;
            if (elapsed >= doms.min_E && elapsed <= doms.max_E) {
                // We found that this start is bound consistent.
                ret_val = start;
                break;
            }
        }
    }

    return ret_val;
}

//int CalendarNoOver::bound_start(const TaskDoms& doms, int p, bool min) const {
//    assert(doms.min_S >= 0);
//    assert(doms.max_S <= size());
//    int ret_val = (min) ? INT_MAX : INT_MIN;
//    int end;
//    int elapsed;
//
//    if (doms.max_S < doms.min_S || doms.max_E < doms.min_E) {
//        return ret_val;
//    }
//
//    int k = 1;
//
//    if (min) {
//        for (int start = next_workable(doms.min_S, any_time); start <= doms.max_S; start = next_workable(start + std::max(1, k), any_time)) {
//            end = get_end(start, p, any_time);
//
//            // If we get out of the calendar, it is clear that we are too far.
//            // Since ending times are excluded from execution window, it is > and not >=.
//            if (end > size()) {
//                break;
//            }
//
//            elapsed = end - start;
//            k = elapsed - doms.max_E;
//            if (elapsed >= doms.min_E && k <= 0) {
//                // We found that this start is bound consistent.
//                ret_val = start;
//                break;
//            }
//        }
//    }
//    else {
//        for (int start = previous_workable(doms.max_S, any_time); start >= doms.min_S; start = previous_workable(start - std::max(1, k), any_time)) {
//            end = get_end(start, p, any_time);
//
//            // If we get out of the calendar, it is clear that we are too far.
//            // Since ending times are excluded from execution window, it is > and not >=.
//            if (end > size()) {
//                continue;
//            }
//
//            elapsed = end - start;
//            k = doms.min_E - elapsed;
//            if (k <= 0 && elapsed <= doms.max_E) {
//                // We found that this start is bound consistent.
//                ret_val = start;
//                break;
//            }
//        }
//    }
//
//    return ret_val;
//}

int CalendarNoOver::bound_elapsed(const TaskDoms& doms, int p, bool min) const {
    assert(doms.min_S >= 0);
    assert(doms.max_S <= size());
    int ret_val = (min) ? INT_MAX : INT_MIN;
    int end;
    int elapsed;

    if (doms.max_S < doms.min_S || doms.max_E < doms.min_E) return ret_val;

    if (min) {
        for (int start = next_workable(doms.min_S, any_time); start <= doms.max_S; start = next_workable(start + 1, regular_hours)) {
            end = get_end(start, p, regular_hours);

            // If we get out of the calendar, it is clear that we are too far.
            // Since ending times are excluded from execution window, it is > and not >=.
            if (end > size()) {
                break;
            }

            elapsed = end - start;
            if (elapsed >= doms.min_E && elapsed <= doms.max_E) {
                if (ret_val > elapsed) ret_val = elapsed;
                if (ret_val == doms.min_E) break;
            }
        }
    }
    else {
        for (int start = next_workable(doms.min_S, any_time); start <= doms.max_S; start = next_workable(start + 1, regular_hours)) {
            end = get_end(start, p, regular_hours);

            // If we get out of the calendar, it is clear that we are too far.
            // Since ending times are excluded from execution window, it is > and not >=.
            if (end > size()) {
                break;
            }
            elapsed = end - start;
            if (elapsed >= doms.min_E && elapsed <= doms.max_E) {
                if (ret_val < elapsed) ret_val = elapsed;
                if (ret_val == doms.max_E) break;
            }
        }
    }

    return ret_val;
}

//int CalendarNoOver::bound_elapsed(const TaskDoms& doms, int p, bool min) const {
//    assert(doms.min_S >= 0);
//    assert(doms.max_S <= size());
//    int ret_val = (min) ? INT_MAX : INT_MIN;
//    int end;
//    int elapsed;
//
//    if (doms.max_S < doms.min_S || doms.max_E < doms.min_E) {
//        return ret_val;
//    }
//
//    int k = 1;
//
//    if (min) {
//        for (int start = next_workable(doms.min_S, any_time); start <= doms.max_S; start = next_workable(start + std::max(1, k), regular_hours)) {
//            end = get_end(start, p, regular_hours);
//
//            // If we get out of the calendar, it is clear that we are too far.
//            // Since ending times are excluded from execution window, it is > and not >=.
//            if (end > size()) {
//                break;
//            }
//
//            elapsed = end - start;
//            k = elapsed - doms.max_E;
//            if (elapsed >= doms.min_E && k <= 0) {
//                if (ret_val > elapsed) {
//                    ret_val = elapsed;
//                }
//                if (ret_val == doms.min_E) {
//                    break;
//                }
//            }
//        }
//    }
//    else {
//        for (int start = next_workable(doms.min_S, any_time); start <= doms.max_S; start = next_workable(start + std::max(1, k), regular_hours)) {
//            end = get_end(start, p, regular_hours);
//
//            // If we get out of the calendar, it is clear that we are too far.
//            // Since ending times are excluded from execution window, it is > and not >=.
//            if (end > size()) {
//                break;
//            }
//            elapsed = end - start;
//            k = elapsed - doms.max_E;
//            if (elapsed >= doms.min_E && k <= 0) {
//                if (ret_val < elapsed) {
//                    ret_val = elapsed;
//                }
//                if (ret_val == doms.max_E) {
//                    break;
//                }
//            }
//        }
//    }
//
//    return ret_val;
//}

int CalendarNoOver::lst(int lct, TaskDoms& doms, int p) const {
    assert(doms.min_S >= 0);
    assert(doms.max_S <= size());
    int ret_val = INT_MIN;
    int rough_lst = get_start(lct, p, any_time);
    int end;
    int elapsed;

    for (int start = previous_workable(std::min(rough_lst, doms.max_S), any_time); start >= doms.min_S; start = previous_workable(start - 1, any_time)) {
        // Since there is no overtime, we know that end is at most lct.
        end = get_end(start, p, regular_hours);

        elapsed = end - start;
        if (elapsed >= doms.min_E && elapsed <= doms.max_E) {
            // We found the lst
            ret_val = start;
            break;
        }
    }
    return ret_val;
}

int CalendarNoOver::ect(int est, TaskDoms& doms, int p) const {
    assert(doms.min_S >= 0);
    assert(doms.max_S <= size());
    int ret_val = INT_MAX;
    int end;
    int elapsed;

    for (int start = next_workable(std::max(doms.min_S, est), any_time); start <= doms.max_S; start = next_workable(start + 1, regular_hours)) {
        end = get_end(start, p, regular_hours);

        // If we get out of the calendar, it is clear that we are too far.
        // Since ending times are excluded from execution window, it is > and not >=.
        if (end > size()) {
            break;
        }

        elapsed = end - start;
        if (elapsed >= doms.min_E && elapsed <= doms.max_E) {
            // We found the ect
            ret_val = end;
            break;
        }
    }
    return ret_val;
}

//int CalendarNoOver::ect(int est, TaskDoms& doms, int p) const {
//    assert(doms.min_S >= 0);
//    assert(doms.max_S <= size());
//    int ret_val = INT_MAX;
//    int end;
//    int elapsed;
//
//    int k = 1;
//
//    for (int start = next_workable(std::max(doms.min_S, est), any_time); start <= doms.max_S; start = next_workable(start + std::max(1, k), regular_hours)) {
//        end = get_end(start, p, regular_hours);
//
//        // If we get out of the calendar, it is clear that we are too far.
//        // Since ending times are excluded from execution window, it is > and not >=.
//        if (end > size()) {
//            break;
//        }
//
//        elapsed = end - start;
//        k = elapsed - doms.max_E;
//        if (elapsed >= doms.min_E && k <= 0) {
//            // We found the ect
//            ret_val = end;
//            break;
//        }
//    }
//    return ret_val;
//}


//======================================================================================================================
// Class CalendarFactory
//======================================================================================================================

CalendarNoOver& CalendarFactory::get_calendar_no_over(const std::vector<int>& cal) {
    auto it = memoized_calendars_no_over.find(cal);
    if (it == memoized_calendars_no_over.end()) {
        memoized_calendars_no_over.emplace(cal, cal);
    }
    return memoized_calendars_no_over.at(cal);
}

CalendarDay& CalendarFactory::get_calendar_day(const std::vector<int>& cal) {
    auto it = memoized_calendars_day.find(cal);
    if (it == memoized_calendars_day.end()) {
        memoized_calendars_day.emplace(cal, cal);
    }
    return memoized_calendars_day.at(cal);
}

CalendarHour& CalendarFactory::get_calendar_hour(const std::vector<int>& cal) {
    auto it = memoized_calendars_hour.find(cal);
    if (it == memoized_calendars_hour.end()) {
        memoized_calendars_hour.emplace(cal, cal);
    }
    return memoized_calendars_hour.at(cal);
}
