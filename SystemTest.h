#pragma once
#include "ConstDefine.h"
#include "Trajectory.h"
#include "Grid.h"
#include "MBB.h"
#include "STIG.h"
#include "FSG.h"
#include "MortonGrid.h"

extern void* baseAddrGPU;


class SystemTest
{
public:
	SystemTest();
	~SystemTest();
	Trajectory* tradb;
	Grid* g;
	STIG* stig;
	FSG* fsg;
	MortonGrid *mgrid;
	MBB rangeQueryMBB;
	int rangeQueryNum;
	int similarityScale;
	int similarityKValue;
	SystemTest(Trajectory* tradb, Grid* g, STIG *stig, FSG* fsg, MortonGrid *mgrid);
	int rangeQueryTest(MBB rangeQueryMBB, int rangeQueryNum);
	int rangeQueryTestWithoutMorton(MBB rangeQueryMBB, int rangeQueryNum);
	int similarityQueryTest(Trajectory t, int similarityScale, int similarityKValue);
	int STIGrangeQueryTest(MBB rangeQueryMBB, int rangeQueryNum);
	int FSGrangeQueryTest(MBB rangeQueryMBB, int rangeQueryNum);
	int MortonGridRangeQueryTest(MBB rangeQueryMBB, int rangeQueryNum);

};

