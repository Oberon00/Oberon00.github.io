#include <stdbool.h>
#include <assert.h>
#include <math.h>

typedef bool(*LessThanFn)(int, int, void*);

int maxInt(int* arr, size_t len, LessThanFn lt, void* state) {
    assert(len > 0);
    int result = arr[0];
    for (size_t i = 1; i < len; ++i) {
        if (lt(result, arr[i], state))
            result = arr[i];
    }

    return result;
}

bool differenceLess(int lhs, int rhs, void* state) {
    int center = *(int*)state;
    return abs(lhs - center) < abs(rhs - center);
}

#define LEN 4

int main() {
    int ints[LEN] = {0, -1, 10, 4};
    int center = 11;
    assert(maxInt(ints, LEN, differenceLess, &center) == -1);
    center = 4;
    assert(maxInt(ints, LEN, differenceLess, &center) == 10);
}

