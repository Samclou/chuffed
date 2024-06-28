//
// Created by Samuel Cloutier on 2023-05-26.
//

#include <chuffed/support/reversible_sparse_set.h>
#include <algorithm>


ReversibleSparseSet::ReversibleSparseSet() : vals(new int[0]), map(new int[0]), size_d(0), lb(0), ub(0) {}

ReversibleSparseSet::ReversibleSparseSet(const vec<int>& _vals) : vals(), map() {
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

ReversibleSparseSet::ReversibleSparseSet(int first, int last) : vals(), map(), lb(first), ub(last) {
    assert(lb <= ub);
    vals = new int[ub - lb + 1];
    map = new int[ub - lb + 1];
    for (int i = 0; i < ub - lb + 1; i++) {
        vals[i] = i;
        map[i] = i;
    }
    size_d = ub - lb + 1;
}

ReversibleSparseSet::~ReversibleSparseSet() {
    delete[] vals;
    delete[] map;
}

bool ReversibleSparseSet::contains(int v) const {
    assert(v >= lb && v <= ub);

    return map[v - lb] < size_d;
}

void ReversibleSparseSet::bind(int v) {
    if (contains(v)) {
        swap(map[v - lb], 0);
        size_d = 1;
    }
    else {
        size_d = 0;
    }

}

void ReversibleSparseSet::remove(int v) {
    if (contains(v)) {
        swap(map[v - lb], --size_d);
    }
}

void ReversibleSparseSet::sort() {
    std::sort(vals, vals + size_d);
    for (int i = 0; i < size_d; i++) {
        map[vals[i] - lb] = i;
    }
}

int ReversibleSparseSet::size() const {
    return size_d;
}

int ReversibleSparseSet::operator[](int index) const {
    assert(0 <= index && index < size_d);
    return vals[index];
}

void ReversibleSparseSet::swap(int i, int j) {
    assert(i >= 0 && i <= ub - lb);
    assert(j >= 0 && j <= ub - lb);
    std::swap(vals[i], vals[j]);
    map[vals[i] - lb] = i;
    map[vals[j] - lb] = j;
}
