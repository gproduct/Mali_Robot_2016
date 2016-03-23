#include <avr/io.h>
#include <util/delay.h>
#include <stdlib.h>
#include "system.h"
#include "odometry.h"
#include "can.h"
#include "usart.h"
#include "sides.h"
#include "actuators.h"
#include "odometry.h"

void TimerHook(void)
{
	if(matchIsStarted() == 1 && getSystemTime() > 90000)
	{
		stop(HARD_STOP);
		while(1);
	}
}
int main(void)
{
	SystemInit();
	
	_delay_ms(100);
    while (1) 
    {
		if(sidesSwitch() == 0)
		{
			purpleSide();			
		}
		else{
			greenSide();
		}

    }
}

