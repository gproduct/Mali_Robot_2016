#ifndef SYSTEM_H_
#define SYSTEM_H_

#define MAX_INPUTS	    20
#define USE_TIMER_HOOK  1


#define GPIOA_BASE 0x22
#define GPIOB_BASE 0x25
#define GPIOC_BASE 0x28
#define GPIOD_BASE 0x2B
#define GPIOE_BASE 0x2E
#define GPIOG_BASE 0x34
#define GPIOF_BASE 0x31

#define PURPLE 1
#define GREEN 0

#define ACTIVATE 1
#define DEACTIVE 0

unsigned char GPIO_PinRegister(volatile unsigned char *baseAddress, unsigned char pin);
unsigned char GPIO_PinRead(unsigned char pinHandler);
unsigned char GPIO_ReadFromRegister(unsigned char pinHandler);
void fillDebaunsingData(void);

void Timer_Init(unsigned int freq);

unsigned char restartCheck;
unsigned char stateRobot;

unsigned long getSystemTime(void);
void SystemInit(void);

//Funkcije za proveru senzora, jumpera i prekidaca
signed char jumperCheck(void);
signed char sidesSwitch(void);
signed char checkLiftSensor(signed char sensor);
signed char checkFrontSensors(signed char sensor);
signed char checkRearSensors(signed char sensor);
signed char checkForStands(signed char sensor);
unsigned char matchIsStarted(void);

#endif
