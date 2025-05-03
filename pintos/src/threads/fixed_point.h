#ifndef __THREAD_FIXED_POINT_H
#define __THREAD_FIXED_POINT_H

/* [MLFQ][Custom] Basic type definition for fixed-point numbers */
typedef int fixed_t;

/* [MLFQ][Custom] Define 16 bits for the fractional part in our fixed-point format */
#define FP_SHIFT_AMOUNT 16

/* [MLFQ][Custom] Convert integer to fixed-point format by shifting left */
#define FP_CONST(A) ((fixed_t)(A << FP_SHIFT_AMOUNT))

/* [MLFQ][Custom] Add two fixed-point numbers */
#define FP_ADD(A,B) (A + B)

/* [MLFQ][Custom] Add a fixed-point number and an integer */
#define FP_ADD_MIX(A,B) (A + (B << FP_SHIFT_AMOUNT))

/* [MLFQ][Custom] Subtract two fixed-point numbers */
#define FP_SUB(A,B) (A - B)

/* [MLFQ][Custom] Subtract an integer from a fixed-point number */
#define FP_SUB_MIX(A,B) (A - (B << FP_SHIFT_AMOUNT))

/* [MLFQ][Custom] Multiply a fixed-point number by an integer */
#define FP_MULT_MIX(A,B) (A * B)

/* [MLFQ][Custom] Divide a fixed-point number by an integer */
#define FP_DIV_MIX(A,B) (A / B)

/* [MLFQ][Custom] Multiply two fixed-point numbers with correct rounding */
#define FP_MULT(A,B) ((fixed_t)(((int64_t) A) * B >> FP_SHIFT_AMOUNT))

/* [MLFQ][Custom] Divide two fixed-point numbers with correct precision */
#define FP_DIV(A,B) ((fixed_t)((((int64_t) A) << FP_SHIFT_AMOUNT) / B))

/* [MLFQ][Custom] Get the integer part of a fixed-point number by shifting right */
#define FP_INT_PART(A) (A >> FP_SHIFT_AMOUNT)

/* [MLFQ][Custom] Round a fixed-point number to nearest integer with different handling for positive and negative values */
#define FP_ROUND(A) (A >= 0 ? ((A + (1 << (FP_SHIFT_AMOUNT - 1))) >> FP_SHIFT_AMOUNT) \
                : ((A - (1 << (FP_SHIFT_AMOUNT - 1))) >> FP_SHIFT_AMOUNT))
#endif /* threads/fixed-point.h */