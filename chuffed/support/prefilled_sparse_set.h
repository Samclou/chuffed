//
// Created by Samuel Cloutier on 2023-06-09.
//

#ifndef CHUFFED_PREFILLEDSPARSESET_H
#define CHUFFED_PREFILLEDSPARSESET_H

#include <chuffed/support/vec.h>

class PrefilledSparseSet {
public:
    PrefilledSparseSet();
    explicit PrefilledSparseSet(const vec<int>& _val);
    PrefilledSparseSet(int first, int last);

    ~PrefilledSparseSet();

    void new_domain(int first, int last);

    bool contains(int v) const;
    void clear();
    void remove(int v);
    void insert(int v);
    void sort(bool (*func)(int, int));
    int size() const;
    int operator[] (int index) const;


private:
    void swap(int i, int j);

    int* vals;
    int* map;
    int size_d;

    int lb;
    int ub;
};


#endif //CHUFFED_PREFILLEDSPARSESET_H
