/*
 * Daxpy
 * Compile:
 * =========
 * 
 * No optimization:
 * $> g++ multiphase_daxpy.cc
 *
 * Create multi-phase workload:
 * $> g++ multiphase_daxpy.cc -Dboston -DBOTH
 *
 * Create only HIGHIPC workload
 * $> g++ multiphase_daxpy.cc -Dboston -DHIGHIPC
 *
 * Create only LOWIPC workload
 * $> g++ multiphase_daxpy.cc -Dboston -DLOWIPC
 *
 * For other manual configurations:
 * $> g++ daxpy12unroll.cc -DUSE_UNROLLING -DUNROLL_COUNT=12 -DARRAYSIZE=10240
 *
 * where -DUSE_UNROLLING sets loop unrolling mode,
 * -DUNROLL_COUNT=x sets loop unrolling count to x,
 * -DARRAYSIZE=x sets array size to x
 *
 * @author: Parth Shah <parth@linux.ibm.com>
 */
#include <iostream>

/*
 * Pre-processing for unrolling loop by UNROLL_COUNT times
 */
#define EMPTY()
#define DEFER(id) id EMPTY()
#define OBSTRUCT(id) id DEFER(EMPTY)()
#define EXPAND(...) __VA_ARGS__

#define EVAL(...)  EVAL1(EVAL1(EVAL1(__VA_ARGS__)))
#define EVAL1(...) EVAL2(EVAL2(EVAL2(__VA_ARGS__)))
#define EVAL2(...) EVAL3(EVAL3(EVAL3(__VA_ARGS__)))
#define EVAL3(...) EVAL4(EVAL4(EVAL4(__VA_ARGS__)))
#define EVAL4(...) EVAL5(EVAL5(EVAL5(__VA_ARGS__)))
#define EVAL5(...) __VA_ARGS__

#define DEC(x) PRIMITIVE_CAT(DEC_, x)
#define DEC_0 0
#define DEC_1 0
#define DEC_2 1
#define DEC_3 2
#define DEC_4 3
#define DEC_5 4
#define DEC_6 5
#define DEC_7 6
#define DEC_8 7
#define DEC_9 8
#define DEC_10 9
#define DEC_11 10
#define DEC_12 11
#define DEC_13 12
#define DEC_14 13
#define DEC_15 14
#define DEC_16 15
#define DEC_17 16
#define DEC_18 17
#define DEC_19 18
#define DEC_20 19

#define CAT(a, ...) PRIMITIVE_CAT(a, __VA_ARGS__)
#define PRIMITIVE_CAT(a, ...) a ## __VA_ARGS__

#define CHECK_N(x, n, ...) n
#define CHECK(...) CHECK_N(__VA_ARGS__, 0,)

#define NOT(x) CHECK(PRIMITIVE_CAT(NOT_, x))
#define NOT_0 ~, 1,

#define COMPL(b) PRIMITIVE_CAT(COMPL_, b)
#define COMPL_0 1
#define COMPL_1 0

#define BOOL(x) COMPL(NOT(x))

#define IF(N) PRIMITIVE_CAT(IF_, N)
#define IF_0(t, ...)
#define IF_1(t, ...) t __VA_ARGS__

#define WHILE(n) \
	IF(BOOL(n)) \
	(	\
		OBSTRUCT(WHILE_REPEAT)() \
		( \
			DEC(n) \
		), \
		prstr(n) \
	)

#define WHILE_REPEAT() WHILE

#ifndef UNROLL_COUNT
#define UNROLL_COUNT 4
#endif

/* Actual code starts from here */
#ifdef USE_UNROLLING
void daxpy(double* x, double* y, int n, double a) {
    for (int i = 0; i < n-UNROLL_COUNT; i += UNROLL_COUNT) {
        y[i]    = a * x[i]    + y[i];
#define prstr(c) y[i+c] = a * x[i+c] + y[i+c];
	EVAL(WHILE(UNROLL_COUNT))
    }
}
#else
void daxpy(double* x, double* y, int n, double a) {
    for (int i = 0; i < n; i++) {
        y[i] = a * x[i] + y[i];
    }
}
#endif

#ifndef ARRAYSIZE
// 1024 KB * 256 (xdouble) = 256MB
#define ARRAYSIZE ((1024*1024))
#endif
#ifndef TIMES
#define TIMES 1000000
#endif


//---------------------------------------------------------
//
//   POWER9 specific optimization- multi-phase workload
//
//---------------------------------------------------------
#ifdef boston
#if defined (LOWIPC) || defined(BOTH)
#define LOWIPC_ARRAYSIZE (1024*2048)
#define LOWIPC_TIMES (390.25*5)
#define LOWIPC_UNROLL_COUNT 1

// O3 is found to be least in IPC for this work on P9 system
#pragma GCC optimize ("O3")
void daxpy_lowipc_p9(double* x, double* y, int n, double a) {
    for (int i = 0; i < n-LOWIPC_UNROLL_COUNT; i += LOWIPC_UNROLL_COUNT) {
        y[i]    = a * x[i]    + y[i];
#define prstr(c) y[i+c] = a * x[i+c] + y[i+c];
	EVAL(WHILE(LOWIPC_UNROLL_COUNT))
    }
}
#endif

#if defined (HIGHIPC) || defined(BOTH)
#define HIGHIPC_ARRAYSIZE (1024*4)
#define HIGHIPC_TIMES (800000*10)
#define HIGHIPC_UNROLL_COUNT 12

#pragma GCC optimize ("O5")
void daxpy_highipc_p9(double* x, double* y, int n, double a) {
    for (int i = 0; i < n-HIGHIPC_UNROLL_COUNT; i += HIGHIPC_UNROLL_COUNT) {
        y[i]    = a * x[i]    + y[i];
#define prstr(c) y[i+c] = a * x[i+c] + y[i+c];
	EVAL(WHILE(HIGHIPC_UNROLL_COUNT))
    }
}
#endif

int main() {
#if defined (LOWIPC) || defined(BOTH)
    std::cout << "size: " << LOWIPC_ARRAYSIZE << ", times: " << LOWIPC_TIMES << std::endl;

    double* low_x = new double[LOWIPC_ARRAYSIZE];
    double* low_y = new double[LOWIPC_ARRAYSIZE];

    for (int i = 0; i < LOWIPC_TIMES; i++)
        daxpy_lowipc_p9(low_x, low_y, LOWIPC_ARRAYSIZE, 1.0);

    // avoid code-removal optimizations
    std::cout << low_y[0] << std::endl;
#endif

#if defined (HIGHIPC) || defined(BOTH)
    std::cout << "size: " << HIGHIPC_ARRAYSIZE << ", times: " << HIGHIPC_TIMES << std::endl;

    double* high_x = new double[HIGHIPC_ARRAYSIZE];
    double* high_y = new double[HIGHIPC_ARRAYSIZE];

    for (int i = 0; i < HIGHIPC_TIMES; i++)
        daxpy_highipc_p9(high_x, high_y, HIGHIPC_ARRAYSIZE, 1.0);

    // avoid code-removal optimizations
    std::cout << high_y[0] << std::endl;
#endif
    
}
//-----------------------------------------------
//
//   generic implementation
//
//-----------------------------------------------
#else
int main() {
    std::cout << "size: " << ARRAYSIZE << ", times: " << TIMES << std::endl;

    double* x = new double[ARRAYSIZE];
    double* y = new double[ARRAYSIZE];

    for (int i = 0; i < TIMES; i++)
        daxpy(x, y, ARRAYSIZE, 1.0);

    // avoid code-removal optimizations
    std::cout << y[0] << std::endl;
    
}   
#endif
