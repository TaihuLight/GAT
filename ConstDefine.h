#pragma once
#define MAX_TRAJ_SIZE 50000
//�ܵ�GPU���õ�shared memory����
#define MAXLENGTH 1024
//MAXGAP�����켣��ʱ�������������������Ӧ�ñ���Ϊ�����켣
#define MAXGAP 3600

#define EPSILON 0.001
#define MAXTHREAD 256

//ÿ��node�ڰ����ĵ�ĸ�������
#define MAXPOINTINNODE 1000

//��FVTable�У�����GPU�Դ����ƣ�ÿ�ο��Լ���FV������
#define N_BATCH_QUERY 2048
#define TRUE 1
#define FALSE 0
#define KSIMILARITY 80

// #define NOT_COLUMN_ORIENTED

#include <stdio.h>
#include <string>
#include <math.h>
#include "QueryResult.h"
#include <cstring>
#include <thread>


#ifdef WIN32
	#include "WinTimer.h"
#else
	#include <sys/time.h>
#endif

//test:��cellΪ�����洢
#define _CELL_BASED_STORAGE
//test:Similarity query based on naive grid���Զ���С��grid������
//#define _SIMILARITY

//4+4+4+4=16bytes
typedef struct Point {
	float x;
	float y;
	int time;
	int tID;
}Point;

//4+4+4=12bytes
typedef struct SPoint {
	float x;
	float y;
	int tID;
}SPoint;

//2+2+4=8bytes
typedef struct DPoint {
	short x;
	short y;
	int tID;
}DPoint;

typedef struct cellBasedTraj {
	int *cellNo = NULL;
	int *startIdx = NULL; //��Ӧ�켣��cell�еĿ�ʼ�Ĺ켣��AllPoints�е�idx
	short *numOfPointInCell = NULL;//��ÿ��cell�ж�Ӧ�ù켣��ĸ���
	short length;
	int trajLength;
}cellBasedTraj;

typedef struct RangeQueryStateTable {
	void* ptr;
	int candidatePointNum;
	float xmin;
	float ymin;
	float xmax;
	float ymax;
	int queryID;
	int startIdxInAllPoints;
}RangeQueryStateTable;

typedef struct OffsetTable {
	int objectId;
	void *addr;
}OffsetTable;

typedef struct TaskInfoTableForSimilarity {
	int qID;
	int candTrajID;
}TaskInfoTableForSimilarity;

typedef struct intPair{
	int int_1;
	int int_2;
}intPair;

#ifdef WIN32
#else


class MyTimer
{
public:
	MyTimer(){
	};
	double iStart;
	double iEnd;

	double cpuSecond() {
		struct timeval tp;
		gettimeofday(&tp, NULL);
		return ((double)tp.tv_sec + (double)tp.tv_usec*1.e-6);
	}

	inline void start()
	{
		iStart = cpuSecond();
	}
	inline void stop()
	{
		iEnd = cpuSecond();
	}
	inline float elapse()
	{
		return iEnd - iStart;
	}
};
#endif

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

int getIdxFromXY(int x, int y);

