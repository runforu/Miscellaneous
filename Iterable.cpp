#include <cassert>
#include <functional>
#include <vector>
#include "Common.h"

namespace Utils {

template <typename Type, typename ElementType, typename SizeType, SizeType (Type::*Count)() const, ElementType& (Type::*Get)(SizeType)>
class Iterable {
    class Iterator {
    public:
        Iterator(Type& container, SizeType index) : m_Container(container), m_Index(index) {
        }

        Iterator(const Iterator& it) : m_Container(it.m_Container), m_Index(it.m_Index) {
        }

        Iterator& operator=(const Iterator& it) {
            this->m_Container = it.m_Container;
            m_Index = it.m_Index;
        }

        bool operator==(const Iterator& it) {
            return m_Index == it.m_Index && &m_Container == &it.m_Container;
        }

        bool operator!=(const Iterator& it) {
            return !(*this == it);
        }

        void operator++() {
            ++m_Index;
        }

        void operator++(int) {
            m_Index++;
        }

        void operator--() {
            --m_Index;
        }

        void operator--(int) {
            m_Index--;
        }

        ElementType& operator*() {
            assert(m_Index >= 0 && m_Index < (m_Container.*Count)());
            return (m_Container.*Get)(m_Index);
        }

    private:
        SizeType m_Index;
        Type& m_Container;
    };

public:
    Iterable(Type& container) : m_Container(container) {
    }

    Iterable(const Iterable& iterable) : m_Container(iterable.m_Container) {
    }

    Iterable& operator=(const Iterable& iterable) {
        this.m_Container = iterable.m_Container;
    }

    Iterator begin() {
        return Iterator(m_Container, 0);
    }

    Iterator end() {
        return Iterator(m_Container, (m_Container.*Count)());
    }

    static auto MakeIterable(Type& container) {
        return Iterable(container);
    }

private:
    Type& m_Container;
};

template <typename Type, typename ElementType, typename SizeType, SizeType (Type::*Count)() const, ElementType& (Type::*Get)(SizeType)>
auto MakeIterable(Type& container) {
    return Iterable<Type, ElementType, SizeType, Count, Get>(container);
}

}  // namespace Utils

namespace Iterable_T {

void TCase0() {
    std::vector<int> vec = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    using size_type = decltype(std::declval<std::vector<int>>().size());
    using element_type = decltype(std::declval<std::vector<int>>().at(size_type{}));
    auto it = Utils::MakeIterable<std::vector<int>, element_type, size_type, &std::vector<int>::size, &std::vector<int>::at>(vec);
    int i = 0;
    for (auto e : it) {
        assert(e == vec[i++]);
    }
}

void TCase1() {
    struct TestClass {
        int* p = nullptr;
        const int SIZE = 10;
        TestClass() {
            p = new int[SIZE]{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        }
        ~TestClass() {
            delete p;
        }
        int GetSize() const {
            return SIZE;
        }
        int& Get(int index) {
            return *(p + index);
        }
    };

    TestClass tc;

    using size_type = decltype(std::declval<TestClass>().GetSize());
    using element_type = decltype(std::declval<TestClass>().Get(size_type{}));
    auto it = Utils::MakeIterable<TestClass, element_type, size_type, &TestClass::GetSize, &TestClass::Get>(tc);
    const auto* element = tc.p;
    for (auto e : it) {
        assert(e == *element++);
    }
}

}  // namespace Iterable_T

void Iterable_Test() {
    UT_Case_ALL(Iterable_T);
}