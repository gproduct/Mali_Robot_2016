#include "system.h"
#include <stdlib.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "fat.h"
#include "actuators.h"

#define _MMIO_BYTE(mem_addr) (*(volatile uint8_t *)(mem_addr))

struct gpio
{
	volatile unsigned char *baseAddress;
	volatile unsigned char pinPosition;
	volatile unsigned char buffer[3];
};

typedef struct gpio GPIOData;

static volatile GPIOData *gpios[MAX_INPUTS];
static volatile unsigned char inputsNumber = 0;
static volatile unsigned long systemTime;
static volatile unsigned char matchStarted = 0;

unsigned int received = 0;



unsigned char GPIO_PinRegister(volatile unsigned char *baseAddress, unsigned char pin)
{
	if(inputsNumber >= MAX_INPUTS)
		return 0;

	unsigned char i;

	gpios[inputsNumber] = (GPIOData *)malloc(sizeof(GPIOData));
	if(gpios[inputsNumber] == NULL)
		return -1;

	gpios[inputsNumber]->baseAddress = baseAddress;
	gpios[inputsNumber]->pinPosition = pin;
	for(i = 0; i < 3; i++)
		gpios[inputsNumber]->buffer[i] = 0;

	/*_MMIO_BYTE(baseAddress - 1) &= (0 << pin);
	_MMIO_BYTE(baseAddress) &= (0 << pin);*/
	_MMIO_BYTE(baseAddress - 1) &= ~(1 << pin);
	_MMIO_BYTE(baseAddress) |= (1 << pin);

	i = inputsNumber;
	inputsNumber++;

	return i;
}

unsigned char GPIO_PinRead(unsigned char pinHandler)
{
	return ( (gpios[pinHandler]->buffer[0]) & (gpios[pinHandler]->buffer[1]) & (gpios[pinHandler]->buffer[2]) );
}

unsigned char GPIO_ReadFromRegister(unsigned char pinHandler)
{
	unsigned char state = 0;

	state = ((_MMIO_BYTE(gpios[pinHandler]->baseAddress - 2)) >> (gpios[pinHandler]->pinPosition)) & 0x01;

	return state;
}

void fillDebaunsingData(void)
{
	unsigned char i;
	static char j = 0;

	if(++j >= 3)
		j = 0;

	for(i = 0; i < inputsNumber; ++i)
		gpios[i]->buffer[j] = GPIO_ReadFromRegister(i);
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

