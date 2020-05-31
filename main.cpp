#include <functional>
#include <iostream>
#include "Common.h"

void begin() {
    std::cout << "begin";
}

void Void() {
    std::cout << "Hello";
}

void end() {
    std::cout << "end";
}

int main() {
    Test(CPolymorphism);
    Test(CommitRollback);
    Test(AsynTask);
    return 0;
}