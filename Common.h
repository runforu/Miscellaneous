#include <assert.h>

#ifndef _COMMON_H_
#define _COMMON_H_

#pragma warning(disable : 4003)

#define TCase(seq, domain) domain::##TCase##seq

#define UT_Case(seq, domain)         \
    __if_exists(TCase(seq, domain)) { \
        TCase(seq, domain)();         \
    }

#define UT_Case_ALL(domain) \
    UT_Case(0, domain);     \
    UT_Case(1, domain);     \
    UT_Case(2, domain);     \
    UT_Case(3, domain);     \
    UT_Case(4, domain);     \
    UT_Case(5, domain);     \
    UT_Case(6, domain);     \
    UT_Case(7, domain);     \
    UT_Case(8, domain);     \
    UT_Case(9, domain);     \
    UT_Case(10, domain);    \
    UT_Case(11, domain);    \
    UT_Case(12, domain);    \
    UT_Case(13, domain);    \
    UT_Case(14, domain);    \
    UT_Case(15, domain);    \
    UT_Case(16, domain);    \
    UT_Case(17, domain);    \
    UT_Case(18, domain);    \
    UT_Case(19, domain);    \
    UT_Case(20, domain);    \
    UT_Case(21, domain);    \
    UT_Case(22, domain);    \
    UT_Case(23, domain);    \
    UT_Case(24, domain);    \
    UT_Case(25, domain);    \
    UT_Case(26, domain);    \
    UT_Case(27, domain);    \
    UT_Case(28, domain);    \
    UT_Case(29, domain);

#define Test(name)             \
    extern void name##_Test(); \
    name##_Test()

#endif  // !_COMMON_H_
