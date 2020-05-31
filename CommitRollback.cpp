#define _CRT_SECURE_NO_WARNINGS
#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include "Common.h"

struct Operator_ {
    Operator_(int t, int v) : type(t), value(v) {
    }
    int type;
    int value;
};

class Solution {
private:
    std::vector<int> data_;
    std::vector<std::vector<Operator_> *> transactions_;

public:
    Solution() {
    }

    void push(int value) {
        data_.push_back(value);
        if (transactions_.size() > 0) {
            std::vector<Operator_> *vo = transactions_.back();
            vo->push_back(Operator_(1, 0));
            return;
        }
    }

    int top() {
        if (data_.size() > 0) {
            return data_.back();
        }
        return 0;
    }

    void pop() {
        if (data_.size() > 0) {
            int v = data_.back();
            data_.pop_back();
            if (transactions_.size() > 0) {
                std::vector<Operator_> *vo = transactions_.back();
                vo->push_back(Operator_(0, v));
                return;
            }
        }
    }

    void begin() {
        std::vector<Operator_> *vo = new std::vector<Operator_>();
        transactions_.push_back(vo);
    }

    bool rollback() {
        if (transactions_.size() == 0) {
            return false;
        }
        std::vector<Operator_> *vo = transactions_.back();
        for (std::vector<Operator_>::iterator it = vo->begin(); it != vo->end(); ++it) {
            if ((*it).type == 0) {
                data_.push_back((*it).value);
            } else {
                if (data_.size() > 0) {
                    data_.pop_back();
                }
            }
        }
        delete vo;
        transactions_.pop_back();
        return true;
    }

    bool commit() {
        if (transactions_.size() == 0) {
            return false;
        }
        std::vector<Operator_> *vo = transactions_.back();
        if (transactions_.size() >= 2) {
            std::vector<Operator_> *vo1 = transactions_.at(transactions_.size() - 2);
            for (std::vector<Operator_>::iterator it = vo->begin(); it != vo->end(); ++it) {
                vo1->push_back(*it);
            }
        }
        delete vo;
        transactions_.pop_back();
        return true;
    }
};

namespace CommitRollback_T {
void TCase0() {
    Solution sol;
    sol.push(5);
    sol.push(2);  // stack: [5,2]
    assert(sol.top() == 2);
    sol.pop();  // stack: [5]
    assert(sol.top() == 5);
    Solution sol2;
    assert(sol2.top() == 0);  // top of an empty stack is 0
    sol2.pop();               // pop should do nothing
}

void TCase1() {
    Solution sol;
    sol.push(4);
    sol.begin();                     // start transaction 1
    sol.push(7);                     // stack: [4,7]
    sol.begin();                     // start transaction 2
    sol.push(2);                     // stack: [4,7,2]
    assert(sol.rollback() == true);  // rollback transaction 2
    assert(sol.top() == 7);          // stack: [4,7]
    sol.begin();                     // start transaction 3
    sol.push(10);                    // stack: [4,7,10]
    assert(sol.commit() == true);    // transaction 3 is committed
    assert(sol.top() == 10);
    assert(sol.rollback() == true);  // rollback transaction 1
    assert(sol.top() == 4);          // stack: [4]
    assert(sol.commit() == false);   // there is no open transaction
}
}  // namespace CommitRollback_T

void CommitRollback_Test() {
    UT_Case_ALL(CommitRollback_T);
}
