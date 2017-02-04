#include "Driver.hpp"

Driver::Driver()
{
	totalLaps = 0;
	numPitstops = 0;
	lapDistance = 0.0;
	bestLapTime = 0.0;
	lastLapTime = 0.0;
	timeIntoLap = 0.0;
	estimatedLapTime = 0.0;
}

Driver::Driver(char pDriverName[32], char pVehicleClass[32], short pTotalLaps, short pNumPitstops, double pLapDistance, double pFullLapDistance, double pBestLapTime, double pLastLapTime, double pTimeIntoLap,
	double pEstimatedLapTime, double pspeed, bool pInPits, bool pIsPlayer, signed char pControl, signed char pFinishStatus, unsigned short pPlace, unsigned char pPitState, 
	double pTimeBehindLeader)
{
	for (int i = 0; i < 32; i++)
	{
		driverName[i] = pDriverName[i];
		vehicleClass[i] = pVehicleClass[i];
	}
	totalLaps = pTotalLaps;
	numPitstops = pNumPitstops;
	lapDistance = pLapDistance;
	totalLapDistance = pTotalLaps + (pLapDistance / pFullLapDistance);
	fullLapDistance = pFullLapDistance;
	bestLapTime = pBestLapTime;
	lastLapTime = pLastLapTime;
	timeIntoLap = pTimeIntoLap;
	estimatedLapTime = pEstimatedLapTime;
	speed = pspeed;
	inPits = pInPits;
	isPlayer = pIsPlayer;
	control = pControl;
	finishStatus = pFinishStatus;
	place = pPlace;
	pitState = pPitState;
	timeBehindLeader = pTimeBehindLeader;
}