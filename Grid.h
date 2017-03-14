#pragma once
#include "Cell.h"
#include "ConstDefine.h"
#include "MBB.h"
#include "QueryResult.h"
#include "Trajectory.h"
#include <iostream>
#include<fstream>
#include<string>
#include<sstream>
#include<vector>

extern Trajectory* tradb;

class Grid
{
public:
	Grid();
	~Grid();
	Grid(const MBB& mbb, float val_cell_size);
	int addTrajectoryIntoCell(Trajectory &t);
	int WhichCellPointIn(SamplePoint p);
	int addDatasetToGrid(Trajectory* db,int traNum);
	int writeCellsToFile(int* cellNo, int cellNum,string file);
	//rangeQuery����������Bounding box������켣��źͶ�Ӧ˳���µĲ�����
	int rangeQuery(MBB & bound, CPURangeQueryResult * ResultTable, int* resultSetSize);
	int rangeQueryGPU(MBB & bound, CPURangeQueryResult * ResultTable, int* resultSetSize);



	//Grid�������������귶Χ
	MBB range;
	float cell_size; //length of a cell
	int cell_num_x,cell_num_y; //�������ж��ٸ�cell
	int cellnum; //upper(area(grid)/area(cell))����֤�ܷ�������cell
	Cell* cellPtr; //�洢cell�����
	ofstream fout;//�ļ�����ӿ�
	int totalPointNum; //grid�ڵ����

	//test:��cellΪ�����������
#ifdef _CELL_BASED_STORAGE
	Point* allPoints;//�洢���е������
	Point* allPointsPtrGPU;

#endif // _CELL_BASED_STORAGE


};

