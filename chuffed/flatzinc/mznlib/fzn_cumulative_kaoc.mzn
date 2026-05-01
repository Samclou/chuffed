%-----------------------------------------------------------------------------%
% Requires that a set of tasks given by start times 's', durations 'd', and
% resource requirements 'r', never require more than a global resource bound
% 'b' at any one time.
% Assumptions:
% - forall i, d[i] >= 0 and r[i] >= 0
%-----------------------------------------------------------------------------%

predicate fzn_cumulative_kaoc(array[int] of var int: s,
                     array[int] of var int: d,
                     array[int] of var int: r, int: b) =
    assert(
        index_set(s) == index_set(d) /\ index_set(s) == index_set(r),
        "cumulative: the 3 array arguments must have identical index sets",
        forall(i in index_set(d))(d[i] >= 0 /\ r[i] >= 0)
	/\  chuffed_cumulative_kaoc(s, d, r, b)
    );

	% Global cumulative propagator
	%
predicate chuffed_cumulative_kaoc(array[int] of var int: s, array[int] of var int: d,
	array[int] of var int: r, int: b);

annotation holes(bool: on_or_off);
annotation kaoc_lifting(bool: on_or_off);

