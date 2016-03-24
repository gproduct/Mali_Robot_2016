#include "system.h"
#include <stdlib.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "actuators.h"


static volatile unsigned char inputsNumber = 0;
static volatile unsigned long systemTime;
static volatile unsigned char matchStarted = 0;

unsigned int received = 0;


void gpio_init(uint8_t pin, uint8_t direction)
{
	uint8_t port = 1 + pin / 8;
	pin = pin % 8;
	if(direction == 1)
	{
		*(volatile uint32_t*)(0x21 + 3 *  (port - 1)) |= (1 << pin);
		*(volatile uint32_t*)(0x22 + 3 *  (port - 1)) &= ~(1 << pin);
	}
	else
		*(volatile uint32_t*)(0x21 + 3 *  (port - 1)) &= ~(1 << pin);

}

void gpio_set(uint8_t pin, uint8_t value)
{
	uint8_t port = 1 + pin / 8;
	pin = pin % 8;
	
	uint8_t temp = *(volatile uint32_t*)(0x20 + 3 * (port - 1));
	if(value != 0)
		*(volatile uint32_t*)(0x22 + 3 * (port - 1)) = temp | (1 << pin);
	else
		*(volatile uint32_t*)(0x22 + 3 * (port - 1)) = temp & ~(1 << pin);
}

void Timer_Init(unsigned int freq)
{
    TCCR1A = 0;
	TCCR1B = (1 << WGM12) | (1 << CS10);
	OCR1A = (double)F_CPU / freq + 0.5;
	TIMSK1 = 1 << OCIE1A;

	SREG |= 0x80;
}


ISR(TIMER1_COMPA_vect)
{
	fillDebaunsingData();
    #if USE_TIMER_HOOK == 1
    TimerHook();
    #endif // USE_TIMER_HOOK
	systemTime++;
}

void SystemInit(void)
{	
	//_delay_ms(1000);
	servo_init(50);
	
	Timer_Init(1000);
	CAN_Init(4);
	
	
	//logger("Initializing digital inputs...\n\r");
	//forwardUpperLeftSensor = GPIO_PinRegister(GPIOA_BASE, 4);//prednji gornji levi senzor za detekciju protivnika		//radi
	
	while(jumperCheck() == 1);
	systemTime = 0;
	matchStarted = 1;
}
unsigned long getSystemTime(void)
{
	return systemTime;
}
/*
signed char checkLiftSensor(signed char sensor)
{
	if(sensor == UP)
	{
		if(upperLiftSensor == 1)
		{
			return SUCCESS;
		}
	}else if(sensor == DOWN)
	{
		if(lowerLiftSensor == 1)
		{
			return SUCCESS;
		}
	}
	
	return FAIL;
	
}//END OF checkLiftSensor

//Funkcija za detektovanje protivnika sa prednje strane
//Prosledjuje se sa koje strane senzor treba aktivirati 
//Moguci su levi, desni, srednji ili svi
//Prosledjuje se LEFT_SIDE, RIGHT_SIDE, MIDDLE i ALL
signed char checkFrontSensors(signed char sensor)
{
	if(sensor == RIGHT_SIDE)
	{
		if(GPIO_PinRead(forwardUpperRightSensor) == 1)
		{
			return DETECTED;
		}
		
	}else if(sensor == LEFT_SIDE)
	{
		if(GPIO_PinRead(forwardUpperLeftSensor) == 1)
		{
			return DETECTED;
		}
		
	}else if(sensor == MIDDLE)
	{
		if(GPIO_PinRead(forwardMiddleSensor) == 0)
		{
			return DETECTED;
		}
	}else if(sensor == ALL)
	{
		if((GPIO_PinRead(forwardUpperRightSensor) == 1) || (GPIO_PinRead(forwardUpperLeftSensor) == 1) || (GPIO_PinRead(forwardMiddleSensor) == 0))
		{
			return DETECTED;	
		}
		
	}
	
	return NOT_DETECTED;
}//END OF checkFrontSensors

//Funkcija za detektovanje protivnika sa zadnje strane
//Prosledjuje se sa koje strane senzor treba aktivirati
//Moduci su levi, desni, srednji ili svi
//Prosledjuje se LEFT_SIDE, RIGHT_SIDE, MIDDLE i ALL
signed char checkRearSensors(signed char sensor)
{
	if(sensor == RIGHT_SIDE)
	{
		if(GPIO_PinRead(backwardRightSensor) == 1)
		{
			return DETECTED;
		}
		
	}else if(sensor == LEFT_SIDE)
	{
		if(GPIO_PinRead(backwardLeftSensor) == 1)
		{
			return DETECTED;
		}
		
	}else if(sensor == MIDDLE)
	{
		if(GPIO_PinRead(backwardMiddleSensor) == 0)
		{
			return DETECTED;
		}
	}else if(sensor == ALL)
	{
		if((GPIO_PinRead(backwardRightSensor) == 1) || (GPIO_PinRead(backwardLeftSensor) == 1) || (GPIO_PinRead(backwardMiddleSensor) == 0))
		{
			return DETECTED;
		}
		
	}
	
	return NOT_DETECTED;
}//END OF checkRearSensors

//Funkcija za detektovanje valjaka
//Prosledjuje se sa koje strane senzor treba aktivirati
//Moguci su levi, desni ili oba
//Prosledjuje se LEFT_SIDE, RIGHT_SIDE ili ALL
signed char checkForStands(signed char sensor)
{
	if(sensor == RIGHT_SIDE)
	{
		if(GPIO_PinRead(forwardLowerRightSensor) == 1)
		{
			return DETECTED;
		}
	}else if(sensor == LEFT_SIDE)
	{
		if(GPIO_PinRead(forwardLowerLeftSensor) == 1)
		{
			return DETECTED;
		}
	}else if(sensor == ALL)
	{
		if((GPIO_PinRead(forwardLowerRightSensor) == 1) || (GPIO_PinRead(forwardLowerLeftSensor) == 1))
		{
			return DETECTED;
		}
	}
	
	return NOT_DETECTED;
}//END OF checkForStands*/

signed char jumperCheck(void)
{
	/*if(GPIO_PinRead(jumper) == 0)
	{
		return 1;
	}
	
	return 0;*/
}

signed char sidesSwitch(void)
{
	if(GPIO_PinRead(sidesSwitch) == 0)
	{
		return 1;
	}
	
	return 0;
}
unsigned char matchIsStarted(void)
{
	return matchStarted;
}

