#ifndef _DRV_MATH_H_
#define _DRV_MATH_H_

#include <regs.h>


void drv_math_init(void);


u32 drv_multiplier_calculation(u32 op1, u32 op2);
s32 drv_div_quotient(u32 op1, u32 op2);
s32 drv_div_remainder(u32 op1, u32 op2);



#endif /* _DRV_MATH_H_ */

