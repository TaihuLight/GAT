#pragma once
#define MAX_TRAJ_SIZE 100000
#define MAXLENGTH 1024
//MAXGAP�����켣��ʱ�������������������Ӧ�ñ���Ϊ�����켣
#define MAXGAP 3600

#include <stdio.h>
#include <string>

//test:��cellΪ�����洢
#define _CELL_BASED_STORAGE


typedef struct Point {
	float x;
	float y;
	uint32_t time;
	uint32_t tID;
}Point;