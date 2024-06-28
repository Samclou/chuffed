//
// Created by Samuel Cloutier on 2023-06-09.
//

#include <chuffed/support/prefilled_sparse_set.h>
#include <algorithm>

PrefilledSparseSet::PrefilledSparseSet() : vals(new int[0]), map(new int[0]), size_d(0), lb(0), ub(0) {}

PrefilledSparseSet::PrefilledSparseSet(const vec<int>& _vals) {
    assert(_vals.size() > 0);
    lb = _vals[0];
    ub = _vals[0];
    for (int i = 1; i < _vals.size(); i++) {
        if (_vals[i] < lb) lb = _vals[i];
        if (_vals[i] > ub) ub = _vals[i];
    }
    vals = new int[ub - lb + 1];
    map = new int[ub - lb + 1];
    vec<bool> in_vals;
    in_vals.growTo(ub - lb + 1);
    for (int i = 0; i < ub - lb + 1; i++) {
        in_vals[i] = false;
    }

    for (int i = 0; i < _vals.size(); i++) {
        if (!in_vals[_vals[i]]) in_vals[_vals[i]] = true;
    }

    int left = 0;
    int right = ub;
    for (int i = 0; i < ub - lb + 1; i++) {
        if (in_vals[i]) {
            vals[left] = i + lb;
            map[i] = left;
            left++;
        }
        else {
            vals[right] = i + lb;
            map[i] = right;
            right--;
        }
    }
    size_d = left;
}

PrefilledSparseSet::PrefilledSparseSet(int first, int last) : lb(first), ub(last) {
    assert(lb <= ub);
    vals = new int[ub - lb + 1];
    map = new int[ub - lb + 1];
    for (int i = 0; i < ub - lb + 1; i++) {
        vals[i] = i;
        map[i] = i;
    }
    size_d = ub - lb + 1;
}

PrefilledSparseSet::~PrefilledSparseSet() {
    delete[] vals;
    delete[] map;
}

void PrefilledSparseSet::new_domain(int first, int last) {
    delete[] vals;
    delete[] map;

    lb = first;
    ub = last;
    assert(lb <= ub);
    vals = new int[ub - lb + 1];
    map = new int[ub - lb + 1];
    for (int i = 0; i < ub - lb + 1; i++) {
        vals[i] = i;
        map[i] = i;
    }
    size_d = ub - lb + 1;
}

bool PrefilledSparseSet::contains(int v) const {
    assert(v >= lb && v <= ub);

    return map[v - lb] < size_d;
}

void PrefilledSparseSet::clear() {
    size_d = 0;
}

void PrefilledSparseSet::remove(int v) {
    if (contains(v)) {
        swap(map[v - lb], --size_d);
    }
}

void PrefilledSparseSet::insert(int v) {
    if (!contains(v)) {
        swap(map[v - lb], size_d++);
    }
}

void PrefilledSparseSet::sort(bool (*func)(int, int)) {
    std::sort(vals, vals + size_d, func);
}

int PrefilledSparseSet::size() const {
    return size_d;
}

int PrefilledSparseSet::operator[](int index) const {
    assert(0 <= index && index < size_d);
    return vals[index];
}

void PrefilledSparseSet::swap(int i, int j) {
    assert(i >= 0 && i <= ub - lb);
    assert(j >= 0 && j <= ub - lb);
    std::swap(vals[i], vals[j]);
    map[vals[i] - lb] = i;
    map[vals[j] - lb] = j;
}