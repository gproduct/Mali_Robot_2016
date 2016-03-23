#define TACTIC_ONE_POSITION_COUNT	1
typedef enum
{
	TACTIC_ONE = 1,
	COLLISION
}robotStates;

typedef struct
{
	position point;
	unsigned char speed;
	signed char direction;
	char (*detectionCallback)(unsigned long startTime);
}gotoFields;

unsigned char numOfStandsRight, numOfStandsLeft;
position positinDetected, compare, starting;
unsigned char activeState;

void greenSide(void);
void purpleSide(void);
