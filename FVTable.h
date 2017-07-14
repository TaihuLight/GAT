#pragma once
#include "ConstDefine.h"
#include<map>
#include<vector>
#include <queue>

using namespace std;

typedef struct FDwithID {
	int traID;
	int FD;
}FDwithID;

struct cmp {
	bool operator()(FDwithID a, FDwithID b) {
		return(a.FD > b.FD);
	}
};

struct cmpBig {
	bool operator()(FDwithID a, FDwithID b) {
		return(a.FD < b.FD);
	}
};

class FVTable
{
public:
	vector<map<int, int>> FreqVector; //ÿ��map����һ���켣
	int trajNum; //�ܹ��Ĺ켣������
	int cellNum; //cell�ĸ���
	void *FVTableGPU, *FVinfoGPU,*queryFVGPU,*FVTableOffset;

	int initFVTable(int trajNum, int cellNum);
	int addPointToFVTable(int trajID, int pointNum, int cellID);
	int getCandidate(int lowerBound, int k, map<int, int>* freqVectorQ, int *candidateTrajID, int *candidateNum);
	double calculateFreqDist(int *freqVectorQ, int trajID);
	int findNeighbor(int cellID, int* neighborID);
	int formPriorityQueue(priority_queue<FDwithID, vector<FDwithID>, cmp> *queue, map<int, int>* freqVectorQ);
	// infoFVGPU����FV��GPU�ڴ洢����Ϣ������FV��GPU����һά������ʽ�洢��ÿ���켣��offset��Ҫ�������Ա�鿴��ִ�������������FV table��GPU�ĵ�ַ�Լ���Ϣ�����ڳ�Ա�ı����ڡ�
	int transferFVtoGPU();
	// pruning on GPU
	int formPriorityQueueGPU(priority_queue<FDwithID, vector<FDwithID>, cmp> *queue, map<int, int>* freqVectorQ);

	FVTable();
	~FVTable();
};

