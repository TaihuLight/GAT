#pragma once
#include "ConstDefine.h"
#include<map>
#include<vector>
#include <queue>

using namespace std;

typedef struct FDwithID {
	int traID;
	double FD;
}FDwithID;

struct cmp {
	bool operator()(FDwithID a, FDwithID b) {
		return(a.FD > b.FD);
	}
};

class FVTable
{
public:
	vector<map<int, int>> FreqVector; //ÿ��map����һ���켣
	int trajNum; //�ܹ��Ĺ켣������
	int cellNum; //cell�ĸ���

	int initFVTable(int trajNum);
	int addPointToFVTable(int trajID, int pointNum, int cellID);
	int getCandidate(int lowerBound, int k, map<int, int>* freqVectorQ, int *candidateTrajID, int *candidateNum);
	double calculateFreqDist(int *freqVectorQ, int trajID);
	int findNeighbor(int cellID, int* neighborID);
	int formPriorityQueue(priority_queue<FDwithID, vector<FDwithID>, cmp> *queue, map<int, int>* freqVectorQ);

	FVTable();
	~FVTable();
};

