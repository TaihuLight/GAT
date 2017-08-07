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
#include "cudaKernel.h"
#include <map>
#include"FVTable.h"


extern Trajectory* tradb;

typedef struct QuadtreeNode {
	int NodeID;
	uint8_t level;
	int numPoints;
	bool isLeaf;
	QuadtreeNode *parent = NULL, *UL = NULL, *UR = NULL, *DL = NULL, *DR = NULL;
	MBB mbb;
}QuadtreeNode;

//��¼node������gpu�ڵ�ַ�Ķ�Ӧ��ϵ�����ڷ�ֹ�ظ�����
typedef struct NodeAddrTable {
	int NodeStartCellID;
	void* ptr;
}NodeAddrTable;

class Grid
{
public:
	Grid();
	~Grid();
	Grid(const MBB& mbb, float val_cell_size);
	int addTrajectoryIntoCell(Trajectory &t);
	int WhichCellPointIn(SamplePoint p);
	int addDatasetToGrid(Trajectory* db,int traNum);
	int writeCellsToFile(int* cellNo, int cellNum, std::string file);
	//rangeQuery����������Bounding box������켣��źͶ�Ӧ˳���µĲ�����
	int rangeQuery(MBB & bound, CPURangeQueryResult * ResultTable, int* resultSetSize);
	int rangeQueryGPU(MBB & bound, CPURangeQueryResult * ResultTable, int* resultSetSize);
	int SimilarityQuery(Trajectory &qTra, Trajectory **candTra, int candSize, float *EDRdistance);
	int buildQuadTree(int level, int id, QuadtreeNode* pNode, QuadtreeNode *parent);
	//rangeQuery����
	int rangeQueryBatch(MBB *bounds, int rangeNum, CPURangeQueryResult *ResultTable, int *resultSetSize);
	int rangeQueryBatchMultiThread(MBB *bounds, int rangeNum, CPURangeQueryResult *ResultTable, int *resultSetSize);
	int findMatchNodeInQuadTree(QuadtreeNode *node, MBB& bound, std::vector<QuadtreeNode*> *cells);
	int rangeQueryBatchGPU(MBB *bounds, int rangeNum, CPURangeQueryResult *ResultTable, int *resultSetSize, RangeQueryStateTable* stateTableAllocate, int device_idx);
	int rangeQueryBatchMultiGPU(MBB *bounds, int rangeNum, CPURangeQueryResult *ResultTable, int *resultSetSize);
	int findMatchNodeInQuadTreeGPU(QuadtreeNode *node, MBB& bound, std::vector<QuadtreeNode*> *cells, cudaStream_t stream, int queryID, int device_idx);
	//SimilarityQuery
	int SimilarityQueryBatch(Trajectory* qTra, int queryTrajNum, int* topKSimilarityTraj, int kValue);
	int SimilarityQueryBatchCPUParallel(Trajectory *qTra, int queryTrajNum, int *EDRdistance, int kValue);
	int SimilarityMultiThreadHandler(std::priority_queue<FDwithID, std::vector<FDwithID>, cmp>* queryQueue, Trajectory* qTra, int queryTrajNum, std::priority_queue<FDwithID, std::vector<FDwithID>, cmpBig>* EDRCalculated, int kValue, int startQueryIdx);
	int FDCalculateParallelHandeler(std::priority_queue<FDwithID, std::vector<FDwithID>, cmp> *queue, std::map<int, int>* freqVectorQ);
	int SimilarityExecuter(SPoint* queryTra, SPoint** candidateTra, int queryLength, int* candidateLength, int candSize, int *resultArray);
	int SimilarityQueryBatchOnGPU(Trajectory * qTra, int queryTrajNum, int * topKSimilarityTraj, int kValue);
	int SimilarityQueryBatchOnMultiGPU(Trajectory * qTra, int queryTrajNum, int * topKSimilarityTraj, int kValue);


	//Grid�������������귶Χ
	MBB range;
	float cell_size; //length of a cell
	int cellNum_axis; // ÿ��/���м���cell
	int cellnum; //upper(area(grid)/area(cell))����֤�ܷ�������cell
	Cell* cellPtr; //�洢cell�����
	std::ofstream fout;//�ļ�����ӿ�
	int totalPointNum; //grid�ڵ����
	int trajNum;
	QuadtreeNode *root;

	std::vector<cellBasedTraj> cellBasedTrajectory; //cellbasedtrajectory����Ԫ�飺��cell��������ַ�����鳤�ȣ�

	SPoint* allPoints;//�洢���е������
	Point* allPointsPtrGPU;
	DPoint *allPointsDeltaEncoding;//Delta Encoding��ĵ�

	//Range Query on GPU ��
	void *baseAddrRange[2];
	void *stateTableGPU[2];
	RangeQueryStateTable* stateTableRange[2];
	std::map<int, void*> nodeAddrTable[2];
	int stateTableLength[2];
	int nodeAddrTableLength[2];

	//Similarity Query ��
	FVTable freqVectors;


};

