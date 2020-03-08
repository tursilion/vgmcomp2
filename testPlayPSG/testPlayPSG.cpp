// testPlayPSG.cpp : This program reads in the PSG data files and plays them
// so you can hear what it currently sounds like.

// We need to read in the channels, and there can be any number of channels.
// They have the following naming schemes:
// voice channels are <name>_tonX.60hz and <name>_volX.60hz
// noise channels are <name>_noiX.60hz and <name>_volX.60hz
// simple ASCII format, values stored as hex (but import should support decimal!), row number is implied frame

// the "60hz" can also be "50hz", "30hz" or "25hz". We want to support all of those here.
// We also want the program to be able to /find/ the channels given a prefix, or
// be given specific channel numbers.

#include "stdafx.h"
#include <stdio.h>

int main()
{

    return 0;
}

