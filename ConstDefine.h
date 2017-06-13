#pragma once
#define MAX_TRAJ_SIZE 50000
#define MAXLENGTH 4096
//MAXGAP�����켣��ʱ�������������������Ӧ�ñ���Ϊ�����켣
#define MAXGAP 3600

#define EPSILON 0.001
#define MAXTHREAD 256

//ÿ��node�ڰ����ĵ�ĸ�������
#define MAXPOINTINNODE 1000

#include <stdio.h>
#include <string>
#include <math.h>
#include "QueryResult.h"

//test:��cellΪ�����洢
#define _CELL_BASED_STORAGE
//test:Similarity query based on naive grid���Զ���С��grid������
//#define _SIMILARITY

//4+4+4+4=16bytes
typedef struct Point {
	float x;
	float y;
	uint32_t time;
	uint32_t tID;
}Point;

//4+4+4=12bytes
typedef struct SPoint {
	float x;
	float y;
	uint32_t tID;
}SPoint;

//2+2+4=8bytes
typedef struct DPoint {
	short x;
	short y;
	uint32_t tID;
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

int getIdxFromXY(int x, int y);

