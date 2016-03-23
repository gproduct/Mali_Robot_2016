#include <avr/io.h>
#include <util/delay.h>
#include <stdlib.h>
#include "system.h"
#include "odometry.h"
#include "can.h"
#include "sides.h"
#include "usart.h"
#include "actuators.h"
#include "fat.h"

/*************************************************************************************************************************************************************************************
																FUNKCIJE, I OSTALE STVARI ZA GREENSIDE
*************************************************************************************************************************************************************************************/




/*************************************************************************************************************************************************************************************
																POZICIJE,BRZINE,SMEROVI I DETEKCIJE ZA ZELENU STRANU
*************************************************************************************************************************************************************************************/
const gotoFields purpleSideTacticOnePositions[TACTIC_ONE_POSITION_COUNT] =
{
	{{460, 1950, 0}, 40, FORWARD, NULL}
};

/*************************************************************************************************************************************************************************************
																				ZELENA STRANA
*************************************************************************************************************************************************************************************/
void purpleSide(void)
{
	position startingPosition;
	unsigned char currentPosition = 0, nextPosition = 0, odometryStatus;
	unsigned char activeState = TACTIC_ONE;
	
	//namestiti startingposition
	startingPosition.x = 0;
	startingPosition.y = 0;
	startingPosition.angle = 0;
	setPosition(startingPosition);
	
	while (1)
	{
		switch(activeState)
		{
			case COLLISION:
				/*if(activeState == TACTIC_ONE && currentPosition == 0)
				{
					_delay_ms(200);
					while(greenSideTacticOnePositions[currentPosition].detectionCallback(0) != 0)
					_delay_ms(100);
					nextPosition = currentPosition;
					activeState = activeState;
					break;
				}*/
			case STUCK:
				_delay_ms(1000);
				activeState = activeState;
				nextPosition = currentPosition;
				break;
			case TACTIC_ONE:
				for(currentPosition = nextPosition;currentPosition < TACTIC_ONE_POSITION_COUNT; currentPosition++)
				{
					odometryStatus = gotoXY(purpleSideTacticOnePositions[currentPosition].point, purpleSideTacticOnePositions[currentPosition].speed,
													purpleSideTacticOnePositions[currentPosition].direction,purpleSideTacticOnePositions[currentPosition].detectionCallback);
					if(odometryStatus = ODOMETRY_FAIL)
					{
						activeState = COLLISION;
						break;
					}
					else if(odometryStatus = ODOMETRY_STUCK)
					{
						
					}
					if(currentPosition == 0)
					{
						
					}
				}//end for
		}//end switch
	}//end while(1)
}//END OF greenSide