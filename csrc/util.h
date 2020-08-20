//
// Created by iffi on 20-8-21.
//

#ifndef SCRATCHPAD_UTIL_H
#define SCRATCHPAD_UTIL_H

#define CEIL(a, b) ( ((a) + (b) - 1) / (b))
#define ALIGN(a, b) (CEIL(a, b) * (b))
#define IN_RANGE(var, low, high) (((var) <= (high)) && ((var) >= (low)))

#endif //SCRATCHPAD_UTIL_H
