%-----------------------------------------------------------------------------%
% Requires that the start time 's', elapsed time 'e' and worked overtime 'o' be
% coherent with a task of processing times 'p' following the hourly calendar
% 'c[w_c]'.
% Assumptions:
% - p >= 0 and (e >= 0 if no calendar is followed, e > 0 otherwise)
%-----------------------------------------------------------------------------%

predicate fzn_calendar_hour(var int: s, var int: o, var int: e, int: p, array[int, int] of int: c, int: w_c) =
    p >= 0 /\
    if w_c = -1 then e = p /\ o = 0 else e > 0 /\ chuffed_calendar_hour(s, o, e, p, [c[w_c, i] | i in index_set_2of2(c)]) endif;

	% Calendar propagator
predicate chuffed_calendar_hour(var int: s, var int: o, var int: e, int: p, array[int] of int: c);
