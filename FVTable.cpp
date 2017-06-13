#include "FVTable.h"
#include <queue>



int FVTable::initFVTable(int trajNum,int cellNum)
{
	this->cellNum = cellNum;
	this->trajNum = trajNum;
	this->FreqVector.resize(trajNum);
	return 0;
}



//��FVTable�����pointNum����
int FVTable::addPointToFVTable(int trajID, int pointNum, int cellID)
{
	map<int, int>::iterator iter = this->FreqVector[trajID].find(cellID);
	if (iter == this->FreqVector[trajID].end()) {
		//û���ҵ������֮
		this->FreqVector[trajID].insert(pair<int, int>(cellID, pointNum));
	}
	else {
		//�ҵ���+1
		iter->second = iter->second + pointNum;
	}
	return 0;
}



int FVTable::getCandidate(int bestNow, int k, map<int,int> * freqVectorQ, int * candidateTrajID, int * candidateNum)
{
	priority_queue<FDwithID, vector<FDwithID>, cmp> FDqueue;
	this->formPriorityQueue(&FDqueue, freqVectorQ);
	for (int i = 0; i <= k - 1; i++) {
		candidateTrajID[i] = FDqueue.top().traID;
		if (candidateTrajID[i] >= bestNow)
			return 1;//û���ҹ�k��
		FDqueue.pop();
		(*candidateNum)++;
	}
	return 0;
}

double FVTable::calculateFreqDist(int * freqVectorQ, int trajID)
{
	return 0.0;
}

int FVTable::findNeighbor(int cellID, int * neighborID)
{
	int x = 0, y = 0;
	for (int bit = 0; bit <= int(log2(this->cellNum)) - 1; bit++) {
		if (bit % 2 == 0) {
			//����λ
			x += ((cellID >> bit)&(1))*(1 << (bit / 2));
		}
		else {
			//ż��λ
			y += ((cellID >> bit)&(1))*(1 << (bit / 2));
		}
	}
	int cnt = 0;
	for (int xx = x - 1; xx <= x + 1; xx++) {
		for (int yy = y - 1; yy <= y + 1; yy++) {
			if ((xx != x) || (yy != y))
				neighborID[cnt++] = getIdxFromXY(xx, yy);
			//printf("%d\t", cnt);
		}
	}
	return 0;
}

int FVTable::formPriorityQueue(priority_queue<FDwithID, vector<FDwithID>, cmp> *queue, map<int, int>* freqVectorQ)
{
	for (int i = 1; i <= this->trajNum - 1; i++) {
		//���ڸù켣���������ѯ��FV��FD
		//���ȣ���������vector�Ķ�ӦԪ�ؼ���
		//ԭʼ�켣��ȥ��ѯ�켣
		int tempVector;// = (int*)malloc(sizeof(int)*this->cellNum);
		map<int, int> tempPositive;
		map<int, int> tempNegative;
		for (map<int, int>::iterator iter = this->FreqVector[i].begin(); iter != this->FreqVector[i].end(); iter++) {
			int cid = iter->first;
			int cfreq = iter->second;
			map<int, int>::iterator iter_query = freqVectorQ->find(cid);
			if (iter_query == freqVectorQ->end()) {
				//˵����query�����Ƶ��Ϊ0
				tempPositive.insert(pair<int, int>(cid, cfreq));
			}
			else {
				tempVector = cfreq - iter_query->second;
				if (tempVector>0)
					tempPositive.insert(pair<int, int>(cid, tempVector));
				else if (tempVector<0)
					tempNegative.insert(pair<int, int>(cid, -tempVector));
			}
		}
		//������ԭʼ�켣��û�е�
		for (map<int, int>::iterator iter = freqVectorQ->begin(); iter != freqVectorQ->end(); iter++) {
			int cid = iter->first;
			//������cid����һ�����账����ˣ�������
			map<int, int>::iterator iter_to_database = this->FreqVector[i].find(cid);
			if (iter_to_database != this->FreqVector[i].end())
				continue;

			//�������˵���ղŵļ�����û�а������cell���������cell�ڲ�ѯ�켣�г����ˣ�ԭʼ�켣��û��
			tempVector = -(iter->second);
			if (tempVector>0)
				tempPositive.insert(pair<int, int>(cid, tempVector));
			else if (tempVector<0)
				tempNegative.insert(pair<int, int>(cid, -tempVector));

		}
		//�������
		//����������map���ڽӵ�cell����
		for (map<int, int>::iterator iter = tempPositive.begin(); iter != tempPositive.end(); iter++) {
			//���ڽӵ�cell
			int cid = iter->first;
			int posiValue = iter->second;
			int neighborIDs[8];
			this->findNeighbor(cid, neighborIDs);
			map<int, int>::iterator iter_neigh;
			for (int i = 0; i <= 7; i++) {
				iter_neigh = tempNegative.find(neighborIDs[i]);
				if (iter_neigh != tempNegative.end()) {
					int negaValue = iter_neigh->second;
					if (posiValue > negaValue) {
						tempPositive[cid] = posiValue - negaValue;
						tempNegative.erase(neighborIDs[i]);
					}
					else if (posiValue == negaValue) {
						tempPositive.erase(cid);
						tempNegative.erase(neighborIDs[i]);
						break;
					}
					else {
						tempNegative[neighborIDs[i]] = negaValue - posiValue;
						tempPositive.erase(cid);
						break;
					}
				}
			}
		}
		for (map<int, int>::iterator iter = tempNegative.begin(); iter != tempNegative.end(); iter++) {
			//���ڽӵ�cell
			int cid = iter->first;
			int negaValue = iter->second;
			int neighborIDs[8];
			this->findNeighbor(cid, neighborIDs);
			map<int, int>::iterator iter_neigh;
			for (int i = 0; i <= 7; i++) {
				iter_neigh = tempPositive.find(neighborIDs[i]);
				if (iter_neigh != tempPositive.end()) {
					int posiValue = iter_neigh->second;
					if (negaValue > posiValue) {
						tempNegative[cid] = negaValue - posiValue;
						tempPositive.erase(neighborIDs[i]);
					}
					else if (posiValue == negaValue) {
						tempNegative.erase(cid);
						tempPositive.erase(neighborIDs[i]);
						break;
					}
					else {
						tempPositive[neighborIDs[i]] = posiValue - negaValue;
						tempNegative.erase(cid);
						break;
					}
				}
			}
		}
		//�ڽ�����������
		int sumPosi = 0, sumNega = 0;
		for (map<int, int>::iterator iter = tempPositive.begin(); iter != tempPositive.end(); iter++)
			sumPosi += iter->second;
		for (map<int, int>::iterator iter = tempNegative.begin(); iter != tempNegative.end(); iter++)
			sumNega += iter->second;
		//ͳ��������ȡ���ֵ�ľ���ֵ
		int resultLB = max(sumPosi, sumNega);
		FDwithID fd;
		fd.traID = i;
		fd.FD = resultLB;
		queue->push(fd);
	}
	return 0;
}

FVTable::FVTable()
{
}


FVTable::~FVTable()
{
}
