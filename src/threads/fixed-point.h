#ifndef FIXED_POINT_H
#define FIXED_POINT_H

#include <stdint.h>

fixedpoint_t fixedpoint_add (fixedpoint_t x , int n);
fixedpoint_t fixedpoint_subtract (fixedpoint_t x , int n);
fixedpoint_t fixedpoint_multiply (fixedpoint_t x , fixedpoint_t y);
fixedpoint_t fixedpoint_divide (fixedpoint_t x , fixedpoint_t y); 
fixedpoint_t fixedpoint_convert_int (int n);
fixedpoint_t fixedpoint_convert_frac (int num, int den);
int fixedpoint_convert_to_int_round (fixedpoint_t x);

#endif