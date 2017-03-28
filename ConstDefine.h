#pragma once
#define MAX_TRAJ_SIZE 100000
#define MAXLENGTH 512
//MAXGAP�����켣��ʱ�������������������Ӧ�ñ���Ϊ�����켣
#define MAXGAP 3600

#define EPSILON 10
#define MAXTHREAD 512

#include <stdio.h>
#include <string>

//test:��cellΪ�����洢
#define _CELL_BASED_STORAGE
//test:Similarity query based on naive grid���Զ���С��grid������
//#define _SIMILARITY

typedef struct Point {
	float x;
	float y;
	uint32_t time;
	uint32_t tID;
}Point;

typedef struct SPoint {
	float x;
	float y;
	uint32_t tID;
}SPoint;
