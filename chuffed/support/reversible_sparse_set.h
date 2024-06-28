//
// Created by Samuel Cloutier on 2023-05-26.
//

#ifndef CHUFFED_REVERSIBLE_SPARSE_SET_H
#define CHUFFED_REVERSIBLE_SPARSE_SET_H

#include <chuffed/core/engine.h>

class ReversibleSparseSet {
public:
    ReversibleSparseSet();
    explicit ReversibleSparseSet(const vec<int>& _val);
    ReversibleSparseSet(int first, int last);

    ~ReversibleSparseSet();

    bool contains(int v) const;
    void bind(int v);
    void remove(int v);
    void sort();
    int size() const;
    int operator[] (int index) const;


private:
    void swap(int i, int j);

    int* vals;
    int* map;
    Tint size_d;

    int lb;
    int ub;
};


#endif //CHUFFED_REVERSIBLE_SPARSE_SET_H
