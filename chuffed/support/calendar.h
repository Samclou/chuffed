//
// Created by Samuel Cloutier on 2023-02-06.
//

#ifndef CHUFFED_CALENDAR_H
#define CHUFFED_CALENDAR_H

#include <vector>
#include <map>
#include <climits>
#include <chuffed/support/vec.h>


enum time_type {regular_hours, overtime_hours, any_time};


struct TaskDoms {
public:
    TaskDoms(int _min_S, int _max_S, int _min_E, int _max_E, int _min_O, int _max_O) : min_S(_min_S), max_S(_max_S),
                                                                                       min_E(_min_E), max_E(_max_E), min_O(_min_O), max_O(_max_O) {}
    TaskDoms(int _min_S, int _max_S, int _min_E, int _max_E) : min_S(_min_S), max_S(_max_S), min_E(_min_E),
                                                               max_E(_max_E), min_O(0), max_O(0) {}
    TaskDoms(const TaskDoms& other) = default;
    int min_S; int max_S;
    int min_E; int max_E;
    int min_O; int max_O;
};


class Calendar {
public:
    virtual int size() const = 0;
    virtual bool workable(int time, time_type type) const = 0;
    virtual int next_workable(int time, time_type type) const = 0;
    virtual int previous_workable(int time, time_type type) const = 0;
    virtual int count_time(int begin, int end, time_type type) const = 0;
    virtual int get_end(int start, int working_time, time_type type) const = 0;
    virtual int get_start(int end, int working_time, time_type type) const = 0;
    virtual int bound_start(const TaskDoms& doms, int p, bool min) const = 0;
    virtual int bound_elapsed(const TaskDoms& doms, int p, bool min) const = 0;
    virtual int bound_over(const TaskDoms& doms, int p, bool min) const = 0;
    virtual int lst(int lct, TaskDoms& doms, int p) const = 0;
    virtual int ect(int est, TaskDoms& doms, int p) const = 0;
};

class CalendarDay : virtual public Calendar {
public:
    explicit CalendarDay(const std::vector<int>& cal);
    int size() const override {return I.size();}
    bool workable(int time, time_type type) const override;
    int next_workable(int time, time_type type) const override;
    int previous_workable(int time, time_type type) const override;
    int count_time(int begin, int end, time_type type) const override;
    int get_end(int start, int working_time, time_type type) const override;
    int get_start(int end, int working_time, time_type type) const override;

    int bound_start(const TaskDoms& doms, int p, bool min) const override;
    int bound_elapsed(const TaskDoms& doms, int p, bool min) const override;
    int bound_over(const TaskDoms& doms, int p, bool min) const override;
    int lst(int lct, TaskDoms& doms, int p) const override;
    int ect(int est, TaskDoms& doms, int p) const override;

private:
    vec<int> I;
    vec<int> X;

    int get_min_end(int start, int min_E, int max_O, int p) const;
    int get_max_end(int start, int max_E, int min_O, int p) const;
};

class CalendarHour : virtual public Calendar {
public:
    explicit CalendarHour(const std::vector<int>& cal);
    int size() const override {return I_reg.size();}
    bool workable(int time, time_type type) const override;
    int next_workable(int time, time_type type) const override;
    int previous_workable(int time, time_type type) const override;
    int count_time(int begin, int end, time_type type) const override;
    int get_end(int begin, int working_time, time_type type) const override;
    int get_start(int end, int working_time, time_type type) const override;

    int bound_start(const TaskDoms& doms, int p, bool min) const override;
    int bound_elapsed(const TaskDoms& doms, int p, bool min) const override;
    int bound_over(const TaskDoms& doms, int p, bool min) const override;
    int lst(int lct, TaskDoms& doms, int p) const override;
    int ect(int est, TaskDoms& doms, int p) const override;

private:
    vec<int> I_reg;
    vec<int> X_reg;
    vec<int> I_over;
    vec<int> X_over;
    vec<int> I_all;
    vec<int> X_all;

    bool verify_head_and_tail(int start, int end, int p) const;
    int get_min_end(int start, int min_E, int max_O, int p) const;
//    int get_min_end(int start, int min_E, int max_E, int max_O, int p) const;
    int get_max_end(int start, int max_E, int min_O, int p) const;
};

class CalendarNoOver : virtual public CalendarDay {
public:
    explicit CalendarNoOver(const std::vector<int>& cal);
    int bound_start(const TaskDoms& doms, int p, bool min) const override;
    int bound_elapsed(const TaskDoms& doms, int p, bool min) const override;
    int bound_over(const TaskDoms& doms, int p, bool min) const override {return 0;};
    int lst(int lct, TaskDoms& doms, int p) const override;
    int ect(int est, TaskDoms& doms, int p) const override;
};

class CalendarFactory {
public:
    static CalendarNoOver& get_calendar_no_over(const std::vector<int>& cal);
    static CalendarDay& get_calendar_day(const std::vector<int>& cal);
    static CalendarHour& get_calendar_hour(const std::vector<int>& cal);

private:
    static std::map<std::vector<int>, CalendarNoOver> memoized_calendars_no_over;
    static std::map<std::vector<int>, CalendarDay> memoized_calendars_day;
    static std::map<std::vector<int>, CalendarHour> memoized_calendars_hour;
};

#endif //CHUFFED_CALENDAR_H
