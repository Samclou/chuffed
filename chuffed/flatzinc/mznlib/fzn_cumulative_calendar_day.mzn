%-----------------------------------------------------------------------------%
% Requires that a set of tasks given by start times 's', worked overtimes 'o',
% elapsed_times 'e', durations 'd', and resource requirements 'r', following
% daily calendars 'cals_followed' defined by day sequences 'cals', number of
% regular hours per day 'n_r', and number of overtime hours per day 'n_o',
% never require more than a global resource bound 'b' at any one time.
% Assumptions:
% - forall i, d[i] >= 0 and r[i] >= 0 and (e[i] >= 0 if i follows no calendar,
%             e[i] > 0 otherwise)
%-----------------------------------------------------------------------------%
include "fzn_calendar_day.mzn";


predicate fzn_cumulative_calendar_day(array[int] of var int: s, array[int] of var int: o, array[int] of var int: e,
                     array[int] of int: d, array[int] of int: r, int: b, array[int, int] of int: cals,
                     array[int] of int: cals_followed, array[int] of int: n_r, array[int] of int: n_o) =
    assert(
        index_set(s) == index_set(o) /\ index_set(s) == index_set(e) /\ index_set(s) == index_set(d)
        /\ index_set(s) == index_set(r) /\ index_set(s) == index_set(cals_followed),
        "cumulative calendar: the array arguments must have compatible index sets",
        forall(i in index_set(d))(d[i] >= 0 /\ r[i] >= 0)
    /\  forall(i in index_set(d))(fzn_calendar_day(s[i], o[i], e[i], d[i], cals, cals_followed[i], n_r[cals_followed[i]+1], n_r[cals_followed[i]+1] + n_o[cals_followed[i]+1]))
	/\  chuffed_cumulative_calendar_day(s, o, e, d, r, b, max(index_set_1of2(cals)) + 1, array1d(cals), cals_followed)
    );


	% Global cumulative calendar propagator
	%
predicate chuffed_cumulative_calendar_day(array[int] of var int: s, array[int] of var int: o,
                     array[int] of var int: e, array[int] of int: d, array[int] of int: r, int: b, int: n_cals,
                     array[int] of int: cals, array[int] of int: cals_followed);

%------------------------------------------------------------------------------
% Options for the alternative cumulative calendar constraint
%   Annotate any cumulative predicate to override Chuffed's defaults for
%   the alternative cumulative calendar propagator.
%
%   - tt_filt_elapsed: TimeTable filtering of the maximum duration

annotation tt_filt_elapsed(bool: on_or_off);
