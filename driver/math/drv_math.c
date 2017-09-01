#include <config.h>
#include "drv_math.h"
//for mutex
#include <rtos.h>


//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------


OsMutex gMathMutex;
#if 0
OsMutex gDivMutex;
#endif

void drv_math_init()
{
	OS_MutexInit(&gMathMutex);
#if 0
	OS_MutexInit(&gDivMutex);
#endif
}

void drv_math_mutex_lock()
{
	OS_MutexLock(gMathMutex);
}

void drv_math_mutex_unlock()
{
	OS_MutexUnLock(gMathMutex);
}

#if 0
void drv_div_mutex_lock()
{
	OS_MutexLock(gDivMutex);
}

void drv_div_mutex_unlock()
{
	OS_MutexUnLock(gDivMutex);
}
#endif

u32 drv_multiplier_calculation(u32 op1, u32 op2)
{
#if (CONFIG_SIM_PLATFORM == 1)
		return (op1*op2);
#else
		u32 ans;

		drv_math_mutex_lock();
		
		SET_MUL_OP1(op1);
		SET_MUL_OP2(op2);
		
		ans = GET_MUL_ANS0;
		
		drv_math_mutex_unlock();	
		return ans;
#endif
}
#if 0            
s32 drv_div_quotient(u32 op1, u32 op2)
{
#if (CONFIG_SIM_PLATFORM == 1)
	return (op1/op2);
#else
	s32 ans;

	drv_div_mutex_unlock();

	SET_DIV_OP1(op1);
	SET_DIV_OP2(op2);

	ans = GET_QUOTIENT;

	drv_div_mutex_unlock();	
	return ans;
#endif
}

s32 drv_div_remainder(u32 op1, u32 op2)
{
#if (CONFIG_SIM_PLATFORM == 1)
	return (op1%op2);
#else
	s32 ans;

	drv_div_mutex_unlock();

	SET_DIV_OP1(op1);
	SET_DIV_OP2(op2);

	ans = GET_REMAINDER;

	drv_div_mutex_unlock();	
	return ans;
#endif
}
#endif




















