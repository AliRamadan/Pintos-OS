#include "threads/fixed-point.h"

// 2^14
fixedpoint_t f = 16384;

fixedpoint_t 
fixedpoint_add (fixedpoint_t x , int n)
{
	return x + n*f;
}

fixedpoint_t
fixedpoint_subtract (fixedpoint_t x , int n)
{
	return x - n*f;
}

fixedpoint_t 
fixedpoint_multiply (fixedpoint_t x , fixedpoint_t y)
{
	return ((int64_t) x) * y / f;
}

fixedpoint_t 
fixedpoint_divide (fixedpoint_t x , fixedpoint_t y)
{
	return ((int64_t) x) * f / y;
}

fixedpoint_t 
fixedpoint_convert_int (int n)
{
	return n * f;
}

fixedpoint_t 
fixedpoint_convert_frac (int num, int den)
{
	return num * f / den;
}

int 
fixedpoint_convert_to_int_round (fixedpoint_t x)
{
	return x >= 0 ? (x + f / 2) / f : (x - f / 2) / f;

}