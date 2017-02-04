#include <iostream>
#include <string>

using namespace std;

#ifndef DRIVER_HPP
#define DRIVER_HPP

class Driver {

public:
	Driver();

	Driver(char[32], char[32], short, short, double, double, double, double, double, double, double, bool, bool, signed char, signed char, unsigned short, unsigned char, double);

	char driverName[32];
	char vehicleClass[32];
	short totalLaps;
	short numPitstops;
	double lapDistance;
	double totalLapDistance;		// totalLaps + lapDistance
	double fullLapDistance;
	double relativeDistance;		// Relative distance to the player
	double bestLapTime;
	double lastLapTime;
	double timeIntoLap;
	double estimatedLapTime;
	double speed;
	double timeBehindLeader;
	double relativeTime;
	bool inPits;
	bool isPlayer;
	signed char control;			// who's in control: -1=nobody (shouldn't get this), 0=local player, 1=local AI, 2=remote, 3=replay (shouldn't get this)
	signed char finishStatus;		// 0=none, 1=finished, 2=dnf, 3=dq
	unsigned short place;
	int placeInClass;
	int classColor;
	unsigned char pitState;			// 0=none, 1=request, 2=entering, 3=stopped, 4=exiting

private:
};

#endif // !DRIVER_HPP
