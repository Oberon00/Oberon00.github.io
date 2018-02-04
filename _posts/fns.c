#include <stdbool.h>
#include <assert.h>

typedef bool(*LessThanFn)(int, int);

int maxInt(int* arr, size_t len, LessThanFn lt) {
    assert(len > 0);
    int result = arr[0];
    for (size_t i = 1; i < len; ++i) {
        if (lt(result, arr[i]))
            result = arr[i];
    }

    return result;
}

bool less(int lhs, int rhs) { return lhs < rhs; }

#define LEN 4

int main() {
  int ints[LEN] = {0, -1, 10, 4};
  assert(maxInt(ints, LEN, less) == 10);
  return 0;
}
