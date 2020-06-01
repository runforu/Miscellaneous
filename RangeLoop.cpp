#include <cassert>
#include <type_traits>
#include "Common.h"

namespace RangeLoop_T {

namespace Case0 {
class Array {
public:
    int mem[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
};

int* begin(Array& A) {
    return &A.mem[0];
}

int* end(Array& A) {
    return &A.mem[std::extent<decltype(Array::mem)>::value];
}

}  // namespace Case0

void TCase0() {
    Case0::Array A{};
    int i = 0;
    for (auto& e : A) {
        assert(e == A.mem[i++]);
    }
}

namespace Case1 {
class Array {
public:
    int mem[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    int* begin() {
        return &mem[0];
    }

    int* end() {
        return &mem[std::extent<decltype(Array::mem)>::value];
    }
};

}  // namespace Case1

void TCase1() {
    Case1::Array A{};
    int i = 0;
    for (auto& e : A) {
        assert(e == A.mem[i++]);
    }
}

}  // namespace RangeLoop_T

void RangeLoop_Test() {
    UT_Case_ALL(RangeLoop_T);
}