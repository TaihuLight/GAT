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

typedef struct QuadtreeNode {
	int NodeID;
	uint8_t level;
	int numPoints;
	bool isLeaf;
	QuadtreeNode *parent = NULL, *UL = NULL, *UR = NULL, *DL = NULL, *DR = NULL;
}QuadtreeNode;

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
	int SimilarityQuery(Trajectory &qTra, Trajectory **candTra, int candSize, float *EDRdistance);
	static int getIdxFromXY(int x, int y);
	int buildQuadTree(int level, int id, QuadtreeNode* pNode, QuadtreeNode *parent);


	//Grid�������������귶Χ
	MBB range;
	float cell_size; //length of a cell
	int cellNum_axis; // ÿ��/���м���cell
	int cellnum; //upper(area(grid)/area(cell))����֤�ܷ�������cell
	Cell* cellPtr; //�洢cell�����
	ofstream fout;//�ļ�����ӿ�
	int totalPointNum; //grid�ڵ����
	QuadtreeNode *root;

	vector<cellBasedTraj> cellBasedTrajectory; //cellbasedtrajectory����Ԫ�飺��cell��������ַ�����鳤�ȣ�

	Point* allPoints;//�洢���е������
	Point* allPointsPtrGPU;
	



};

