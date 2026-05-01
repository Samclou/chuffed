#include <cstdint>

template <class T = uint64_t, bool is_knapsack = true>
class BitSet {
    int size;
    int max_val;

    int block_size = CHAR_BIT * sizeof(T);
    int n_block;

    T* values;
    int block_for_mask;
    T mask;

public:
    explicit BitSet(int _capacity, int _max_value): size(_capacity), max_val(_max_value) {
        if (is_knapsack) {
            assert(_capacity >= max_val);
            assert(max_val >= 0);
            n_block = (size + block_size - 1) / block_size;
            values = new T[n_block];

            block_for_mask = max_val / block_size;
            mask = T(1) << max_val % block_size;
            mask--;
            mask <<= 1;
            mask++;

            reset();
        }
        else {
            mask = 0;
        }
    }

    ~BitSet() {
        if (is_knapsack) {
            delete[] values;
        }
    }

    int get_max() {
        return max_val;
    }

    void new_max(int _max_value) {
        assert(size >= _max_value);
        assert(_max_value >= 0);
        max_val = _max_value;
        if (!is_knapsack) {
            return;
        }

        block_for_mask = max_val / block_size;
        mask = T(1) << max_val % block_size;
        mask--;
        mask <<= 1;
        mask++;
    }

    void reset() {
        if (!is_knapsack) {
            mask = 0;
            return;
        }
        for (int i = n_block - 1; i >= 0; i--) {
            values[i] = 0;
        }
    }

    void reset_to_one() {
        if (!is_knapsack) {
            mask = 0;
            return;
        }
        for (int i = n_block - 1; i >= 0; i--) {
            values[i] = 0;
        }
        values[0]++;
    }

    void copy_values(const BitSet<T, is_knapsack>& other) {
        if (!is_knapsack) {
            mask = other.mask;
            return;
        }
        assert(size == other.size);
        for (int i = n_block - 1; i >= 0; i--) {
            values[i] = other.values[i];
        }
    }

    void add_item(int shift) {
        assert(shift >= 0);
        if (!is_knapsack) {
            mask += shift;
            return;
        }
        int in_block_shift = shift % block_size;
        int block_shift = shift / block_size;

        int i = n_block - 1;
        if (in_block_shift == 0) {
            if (block_shift == 0) {
                return;
            }
            while(i >= block_shift) {
                values[i] |= values[i - block_shift];
                i--;
            }
        }
        else {
            while(i >= block_shift + 1) {
                values[i] |= values[i - block_shift] << in_block_shift;
                values[i] |= values[i - block_shift - 1] >> (block_size - in_block_shift);
                i--;
            }
            if (i >= block_shift) {
                values[i] |= values[i - block_shift] << in_block_shift;
            }
        }
    }

    int find_msb() const {
        if (!is_knapsack) {
            if (max_val < mask) {
                return max_val;
            }
            return int(mask);
        }
        int i = block_for_mask;
        if ((values[i] & mask) > 0) {
            return (i * block_size) + (block_size - __builtin_clzl(values[i] & mask) - 1);
        }
        i--;
        while (i >= 0) {
            if (values[i] > 0) {
                return (i * block_size) + (block_size - __builtin_clzl(values[i]) - 1);
            }
            i--;
        }
        return -1;
    }

    bool operator==(const BitSet<T, is_knapsack>& other) const {
        if (size != other.size || max_val != other.max_val) {
            return false;
        }
        if (!is_knapsack) {
            return mask == other.mask;
        }
        for (int i = 0; i < n_block; i++) {
            if (other.values[i] != values[i]) {
                return false;
            }
        }
        return true;
    }
};

