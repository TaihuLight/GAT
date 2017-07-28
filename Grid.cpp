#include "Grid.h"

extern Trajectory* tradb;
extern void* baseAddrGPU;
#ifdef WIN32
MyTimer timer;
#else

#endif

Grid::Grid()
{
	range = MBB(0, 0, 0, 0);
	cellnum = 0;
	cell_size = 0;
	cellNum_axis = 0;
	cellPtr = NULL;
	QuadtreeNode* root;
	allPoints = NULL;
	allPointsPtrGPU = NULL;
}

//���Թ���û����
int getIdxFromXY(int x, int y)
{
	int lenx, leny;
	if (x == 0)
		lenx = 1;
	else
	{
		lenx = int(log2(x)) + 1;
	}
	if (y == 0)
		leny = 1;
	else
		leny = int(log2(y)) + 1;
	int result = 0;
	int xbit = 1, ybit = 1;
	for (int i = 1; i <= 2 * max(lenx, leny); i++)
	{
		if ((i & 1) == 1) //����
		{
			result += (x >> (xbit - 1) & 1) * (1 << (i - 1));
			xbit = xbit + 1;
		}
		else //ż��
		{
			result += (y >> (ybit - 1) & 1) * (1 << (i - 1));
			ybit = ybit + 1;
		}
	}
	return result;
}

int Grid::buildQuadTree(int level, int id, QuadtreeNode* pNode, QuadtreeNode* parent)
{
	int totalLevel = int(log2(this->cellnum) / log2(4));
	int totalPoints = 0;
	for (int i = id * int(pow(4, (totalLevel - level))); i <= (id + 1) * int(pow(4, (totalLevel - level))) - 1; i++)
	{
		totalPoints += this->cellPtr[i].totalPointNum;
	}
	pNode->mbb = MBB(this->cellPtr[id * int(pow(4, (totalLevel - level)))].mbb.xmin, this->cellPtr[(id + 1) * int(pow(4, (totalLevel - level))) - 1].mbb.ymin, this->cellPtr[(id + 1) * int(pow(4, (totalLevel - level))) - 1].mbb.xmax, this->cellPtr[id * int(pow(4, (totalLevel - level)))].mbb.ymax);
	pNode->numPoints = totalPoints;
	pNode->NodeID = id;
	pNode->parent = parent;
	pNode->level = level;
	if ((totalPoints < MAXPOINTINNODE) || (level == totalLevel))
	{
		pNode->isLeaf = true;
		pNode->DL = NULL;
		pNode->DR = NULL;
		pNode->UL = NULL;
		pNode->UR = NULL;
		return 0;
	}
	else
	{
		pNode->isLeaf = false;
		pNode->UL = (QuadtreeNode*)malloc(sizeof(QuadtreeNode));
		this->buildQuadTree(level + 1, id << 2, pNode->UL, pNode);
		pNode->UR = (QuadtreeNode*)malloc(sizeof(QuadtreeNode));
		this->buildQuadTree(level + 1, (id << 2) + 1, pNode->UR, pNode);
		pNode->DL = (QuadtreeNode*)malloc(sizeof(QuadtreeNode));
		this->buildQuadTree(level + 1, (id << 2) + 2, pNode->DL, pNode);
		pNode->DR = (QuadtreeNode*)malloc(sizeof(QuadtreeNode));
		this->buildQuadTree(level + 1, (id << 2) + 3, pNode->DR, pNode);
		return 0;
	}
}


Grid::Grid(const MBB& mbb, float val_cell_size)
{
	range = mbb;
	cell_size = val_cell_size;
	//ò��ֻ��Ҫ��һ��ά�Ⱦ����ˣ���Ϊ�涨���˱�����2*2,4*4������
	int divideNumOnX = (int)((mbb.xmax - mbb.xmin) / val_cell_size) + 1; //����Ҫ�ö��ٸ�cell
	int divideNumOnY = (int)((mbb.ymax - mbb.ymin) / val_cell_size) + 1;
	int maxValue = max(divideNumOnX, divideNumOnY);
	//�ҵ���ѵĳ���
	cellNum_axis = maxValue >> (int(log2(maxValue))) << (int(log2(maxValue)) + 1);
	cellnum = cellNum_axis * cellNum_axis;
	cellPtr = new Cell[cellnum];
	//����������������Ҫ����xmax��ymin������չrange

	//ע��cell����Ǵ�(xmin,ymax)��ʼ�ģ�������(xmin,ymin)
	//Z���α���
	for (int i = 0; i <= cellNum_axis - 1; i++)
	{
		for (int j = 0; j <= cellNum_axis - 1; j++)
		{
			int cell_idx = getIdxFromXY(j, i);
			cellPtr[cell_idx].initial(i, j, MBB(range.xmin + cell_size * j, range.ymax - cell_size * (i + 1), range.xmin + cell_size * (j + 1), range.ymax - cell_size * (i)));
		}
	}
}

//�ѹ켣t������ӹ켣����ӵ�cell����
//��һ�������ǰ��ӹ켣�Ž���cell���棬���һ����item
int Grid::addTrajectoryIntoCell(Trajectory& t)
{
	if (t.length == 0)
		return 1;//�չ켣
	SamplePoint p = t.points[0];
	int lastCellNo = WhichCellPointIn(p);
	int lastCellStartIdx = 0;
	int nowCellNo;
	//cell based traj���ɣ��ǵ�ת����free��
	vector<int>* tempCellBasedTraj = new vector<int>;
	tempCellBasedTraj->reserve(1048577);
	int tempCellNum = 0;
	for (int i = 0; i <= t.length - 1; i++)
	{
		p = t.points[i];
		nowCellNo = WhichCellPointIn(p);
		if (i == t.length - 1)
		{
			//�����һ�����������cellҲ���ϸ�cell�������һ��cell�ˣ����֮
			if (lastCellNo == nowCellNo)
			{
				tempCellNum++;
				tempCellBasedTraj->push_back(nowCellNo);
				cellPtr[nowCellNo].addSubTra(t.tid, lastCellStartIdx, i, i - lastCellStartIdx + 1);
				this->freqVectors.addPointToFVTable(t.tid, i - lastCellStartIdx + 1, nowCellNo);
			}
			//������һ�������cell��Ҫ���
			else
			{
				tempCellNum += 2;
				tempCellBasedTraj->push_back(lastCellNo);
				tempCellBasedTraj->push_back(nowCellNo);
				cellPtr[lastCellNo].addSubTra(t.tid, lastCellStartIdx, i - 1, i - 1 - lastCellStartIdx + 1);
				this->freqVectors.addPointToFVTable(t.tid, i - 1 - lastCellStartIdx + 1, lastCellNo);
				cellPtr[nowCellNo].addSubTra(t.tid, i, i, 1);
				this->freqVectors.addPointToFVTable(t.tid, 1, nowCellNo);
			}
		}
		else
		{
			if (lastCellNo == nowCellNo)
				continue;
			else
			{
				// �ս�һ���ӹ켣����ʼ��һ���ӹ켣
				//cellTra�����һ��
				tempCellNum++;
				tempCellBasedTraj->push_back(lastCellNo);
				//SubTra���
				//printf("cell:%d\n", lastCellNo);
				cellPtr[lastCellNo].addSubTra(t.tid, lastCellStartIdx, i - 1, i - 1 - lastCellStartIdx + 1);
				
				this->freqVectors.addPointToFVTable(t.tid, i - 1 - lastCellStartIdx + 1, lastCellNo);
				lastCellNo = nowCellNo;
				lastCellStartIdx = i;
			}
		}
	}
	this->cellBasedTrajectory[t.tid].length = tempCellNum;
	this->cellBasedTrajectory[t.tid].cellNo = (int*)malloc(sizeof(int) * tempCellNum);
	if (this->cellBasedTrajectory[t.tid].cellNo == NULL) throw("alloc error");
	for (int i = 0; i <= tempCellNum - 1; i++)
	{
		this->cellBasedTrajectory[t.tid].cellNo[i] = tempCellBasedTraj->at(i);
	}
	this->cellBasedTrajectory[t.tid].trajLength = t.length;
	delete tempCellBasedTraj;
	return 0;
}

//ȷ������
int Grid::WhichCellPointIn(SamplePoint p)
{
	//ע��cell����Ǵ�(xmin,ymax)��ʼ�ģ�������(xmin,ymin)
	int row = (int)((range.ymax - p.lat) / cell_size); //��0��ʼ
	int col = (int)((p.lon - range.xmin) / cell_size); //��0��ʼ
	return getIdxFromXY(col, row);
}

int Grid::addDatasetToGrid(Trajectory* db, int traNum)
{
	this->trajNum = traNum;
	//����frequency vector
	this->freqVectors.initFVTable(traNum, this->cellnum);
	//ע�⣬�켣��Ŵ�1��ʼ
	this->cellBasedTrajectory.resize(traNum + 1); //����cellbasedtraj�Ĺ�ģ���ӹ켣��ʱ�����ֱ����
	int pointCount = 0;
	for (int i = 1; i <= traNum; i++)
	{
		addTrajectoryIntoCell(db[i]);
	}
	for (int i = 0; i <= cellnum - 1; i++)
	{
		cellPtr[i].buildSubTraTable();
		pointCount += cellPtr[i].totalPointNum;
	}
	this->totalPointNum = pointCount;
	//������������
	//subTraTable�����Ǽ�¼���ӹ켣����ʼoffset����ֹoffset��Tid��

	//����Quadtree���Զ����½������ָ�ڵ�ʹ���нڵ������ĸ���С��MAXPOINTINNODE
	this->root = (QuadtreeNode*)malloc(sizeof(QuadtreeNode));
	this->buildQuadTree(0, 0, this->root, NULL);

	//ת��Ϊcell�����洢
	//�˴������洢��ָͬһcell�ڵĲ�����洢��һ��������rangeQuery����������similarity query
	//similarity��װ�켣��ʱ�򣬿����ȼ�¼��ǰ�ǵڼ���subtra���ҹ켣��ʱ�����������ң�����tid�ظ����ڵ�����
	this->allPoints = (SPoint*)malloc(sizeof(SPoint) * (this->totalPointNum));
	pointCount = 0;


	for (int i = 0; i <= cellnum - 1; i++)
	{
		cellPtr[i].pointRangeStart = pointCount;
		for (int j = 0; j <= cellPtr[i].subTraNum - 1; j++)
		{
			//for each subTra, add Points to AllPoints
			cellPtr[i].subTraTable[j].idxInAllPointsArray = pointCount;
			for (int k = cellPtr[i].subTraTable[j].startpID; k <= cellPtr[i].subTraTable[j].endpID; k++)
			{
				allPoints[pointCount].tID = cellPtr[i].subTraTable[j].traID;
				allPoints[pointCount].x = tradb[allPoints[pointCount].tID].points[k].lon;
				allPoints[pointCount].y = tradb[allPoints[pointCount].tID].points[k].lat;
				//allPoints[pointCount].time = tradb[allPoints[pointCount].tID].points[k].time;
				pointCount++;
			}
		}
		cellPtr[i].pointRangeEnd = pointCount - 1;
		if (cellPtr[i].pointRangeEnd - cellPtr[i].pointRangeStart + 1 != cellPtr[i].totalPointNum)
			cerr << "Grid.cpp: something wrong in total point statistic" << endl;
	}
	//����CellBasedTrajectory����Ҫ�����idx��Ϣ
	//��ÿ���켣����cellBasedTrajectory��������Ϣ���ڴ棺��ʼλ�úͲ�������Ŀ
	for (int i = 1; i <= this->trajNum; i++)
	{
		this->cellBasedTrajectory[i].startIdx = (int*)malloc(sizeof(int) * this->cellBasedTrajectory[i].length);
		this->cellBasedTrajectory[i].numOfPointInCell = (short*)malloc(sizeof(short) * this->cellBasedTrajectory[i].length);
		int* tempCntForTraj = (int*)malloc(sizeof(int) * this->cellnum);
		memset(tempCntForTraj, 0, sizeof(int) * this->cellnum);
		for (int cellidx = 0; cellidx <= this->cellBasedTrajectory[i].length - 1; cellidx++)
		{
			int nowCellID = this->cellBasedTrajectory[i].cellNo[cellidx];
			int j, cnt;
			for (j = 0 , cnt = 0; cnt <= tempCntForTraj[nowCellID]; j++)
			{
				if (this->cellPtr[nowCellID].subTraTable[j].traID == i)
				{
					cnt++;
				}
			}
			j--;
			//choose j; //ѡ���j��subTra�����������滹��û����ʼ�����Ϣ�����ǻ�����point����Ϣ�ӽ�subtraTable�������������Ҫ��subTraTable��free��
			this->cellBasedTrajectory[i].startIdx[cellidx] = this->cellPtr[nowCellID].subTraTable[j].idxInAllPointsArray;
			this->cellBasedTrajectory[i].numOfPointInCell[cellidx] = this->cellPtr[nowCellID].subTraTable[j].numOfPoint;
			tempCntForTraj[nowCellID]++;
		}
		free(tempCntForTraj);
	}
	// Transfer FV to GPU
	//this->freqVectors.transferFVtoGPU();


	////Delta Encoding��cell�����洢
	//this->allPointsDeltaEncoding = (DPoint*)malloc(sizeof(DPoint)*(this->totalPointNum));
	//pointCount = 0;
	//for (int i = 0; i <= cellnum - 1; i++) {
	//	cellPtr[i].pointRangeStart = pointCount;
	//	for (int j = 0; j <= cellPtr[i].subTraNum - 1; j++) {
	//		for (int k = cellPtr[i].subTraTable[j].startpID; k <= cellPtr[i].subTraTable[j].endpID; k++) {
	//			allPointsDeltaEncoding[pointCount].tID = cellPtr[i].subTraTable[j].traID;
	//			allPointsDeltaEncoding[pointCount].x = short(int((tradb[allPointsDeltaEncoding[pointCount].tID].points[k].lon)*1000000)-cellPtr[i].anchorPointX);
	//			allPointsDeltaEncoding[pointCount].y = short(int((tradb[allPointsDeltaEncoding[pointCount].tID].points[k].lat)*1000000)-cellPtr[i].anchorPointY);
	//			pointCount++;
	//		}
	//	}
	//	cellPtr[i].pointRangeEnd = pointCount - 1;
	//	if (cellPtr[i].pointRangeEnd - cellPtr[i].pointRangeStart + 1 != cellPtr[i].totalPointNum)
	//		cerr << "Grid.cpp: something wrong in total point statistic" << endl;
	//}

	//�����ɺõ�allpoints�ŵ�GPU��
	//putCellDataSetIntoGPU(this->allPoints, this->allPointsPtrGPU, this->totalPointNum);


	return 0;
}

int Grid::writeCellsToFile(int* cellNo, int cellNum, string file)
// under editing....
{
	fout.open(file, ios_base::out);
	for (int i = 0; i <= cellNum - 1; i++)
	{
		int outCellIdx = cellNo[i];
		cout << outCellIdx << ": " << "[" << cellPtr[outCellIdx].mbb.xmin << "," << cellPtr[outCellIdx].mbb.xmax << "," << cellPtr[outCellIdx].mbb.ymin << "," << cellPtr[outCellIdx].mbb.ymax << "]" << endl;
		for (int j = 0; j <= cellPtr[outCellIdx].subTraNum - 1; j++)
		{
			int tid = cellPtr[outCellIdx].subTraTable[j].traID;
			int startpid = cellPtr[outCellIdx].subTraTable[j].startpID;
			int endpid = cellPtr[outCellIdx].subTraTable[j].endpID;
			for (int k = startpid; k <= endpid; k++)
			{
				cout << tradb[tid].points[k].lat << "," << tradb[tid].points[k].lon << ";";
			}
			cout << endl;
		}
	}
	return 0;
}

int Grid::rangeQueryBatch(MBB* bounds, int rangeNum, CPURangeQueryResult* ResultTable, int* resultSetSize)
{
	ofstream out("queryResult.txt", ios::out);
	ResultTable = (CPURangeQueryResult*)malloc(sizeof(CPURangeQueryResult));
	ResultTable->traid = -1; //table��ͷtraidΪ-1 flag
	ResultTable->next = NULL;
	resultSetSize = (int*)malloc(sizeof(int) * rangeNum);
	CPURangeQueryResult  *nowResult;
	nowResult = ResultTable;
	int totalLevel = int(log2(this->cellnum) / log2(4));
	for (int i = 0; i <= rangeNum - 1; i++)
	{
		//int candidateNodeNum = 0;
		resultSetSize[i] = 0;
		vector<QuadtreeNode*> cells;
		findMatchNodeInQuadTree(this->root, bounds[i], &cells);
		//printf("%d", cells.size());
		for (vector<QuadtreeNode*>::iterator iterV = cells.begin(); iterV != cells.end(); iterV++)
		{
			int nodeID = (*iterV)->NodeID;
			int nodeLevel = (*iterV)->level;
			int firstCellID = nodeID * int(pow(4, (totalLevel - nodeLevel)));
			int lastCellID = (nodeID + 1) * int(pow(4, (totalLevel - nodeLevel))) - 1;
			for (int cellID = firstCellID; cellID <= lastCellID; cellID++)
			{
				int anchorX = this->cellPtr[cellID].anchorPointX;
				int anchorY = this->cellPtr[cellID].anchorPointY;
				for (int idx = this->cellPtr[cellID].pointRangeStart; idx <= this->cellPtr[cellID].pointRangeEnd; idx++)
				{
					//compress
					//float realX = float(allPointsDeltaEncoding[idx].x + anchorX) / 1000000;
					//float realY = float(allPointsDeltaEncoding[idx].y + anchorY) / 1000000;
					// no compress
					float realX = allPoints[idx].x;
					float realY = allPoints[idx].y;
					if (bounds[i].pInBox(realX, realY))
					{
						//printf("%f,%f", realX, realY);
						//printf("%d\n", idx);
						//newResult = (CPURangeQueryResult*)malloc(sizeof(CPURangeQueryResult));
						//if (newResult == NULL)
						//	return 2; //�����ڴ�ʧ��
						//compress
						//newResult->traid = allPointsDeltaEncoding[idx].tID;
						//no compress
						//newResult->traid = allPoints[idx].tID;
						//newResult->x = realX;
						//newResult->y = realY;
						//// out << "Qid:" << i << "......." << newResult->x << "," << newResult->y << endl;
						//newResult->next = NULL;
						//nowResult->next = newResult;
						//nowResult = newResult;
						resultSetSize[i]++;
					}
				}
			}
		}
	}
	out.close();
	return 0;
}

int Grid::findMatchNodeInQuadTree(QuadtreeNode* node, MBB& bound, vector<QuadtreeNode*>* cells)
{
	if (node->isLeaf)
	{
		cells->push_back(node);
	}
	else
	{
		if (bound.intersect(node->UL->mbb))
			findMatchNodeInQuadTree(node->UL, bound, cells);
		if (bound.intersect(node->UR->mbb))
			findMatchNodeInQuadTree(node->UR, bound, cells);
		if (bound.intersect(node->DL->mbb))
			findMatchNodeInQuadTree(node->DL, bound, cells);
		if (bound.intersect(node->DR->mbb))
			findMatchNodeInQuadTree(node->DR, bound, cells);
	}
	return 0;
}


int Grid::rangeQueryBatchGPU(MBB* bounds, int rangeNum, CPURangeQueryResult* ResultTable, int* resultSetSize)
{
	// ����GPU�ڴ�
	//MyTimer timer;
	// ����������õģ������ٵ�
	//timer.start();

	RangeQueryStateTable* stateTableAllocate = (RangeQueryStateTable*)malloc(sizeof(RangeQueryStateTable) * 1000000);
	this->stateTableRange = stateTableAllocate;
	this->stateTableLength = 0;
	this->nodeAddrTableLength = 0;
	// for each query, generate the nodes:
	cudaStream_t stream;
	cudaStreamCreate(&stream);
	for (int i = 0; i <= rangeNum - 1; i++)
	{
		findMatchNodeInQuadTreeGPU(root, bounds[i], NULL, stream, i);
	}
	//printf("StateTableLength:%d",this->stateTableLength);
	//stateTable�е����Ŀ�����ֵ
	int maxPointNum = 0;
	for (int i = 0; i <= stateTableLength - 1; i++)
	{
		if (stateTableAllocate[i].candidatePointNum > maxPointNum)
			maxPointNum = stateTableAllocate[i].candidatePointNum;
	}
	//����GPU���в��в�ѯ
	//�ȴ���stateTable
	//timer.stop();
	//cout << "Time 1:" << timer.elapse() << "ms" << endl;

	//timer.start();
	RangeQueryStateTable* stateTableGPU = NULL;
	CUDA_CALL(cudaMalloc((void**)&stateTableGPU, sizeof(RangeQueryStateTable)*this->stateTableLength));
	CUDA_CALL(cudaMemcpyAsync(stateTableGPU, stateTableAllocate, sizeof(RangeQueryStateTable)*this->stateTableLength,
		cudaMemcpyHostToDevice, stream));
	//������ɣ���ʼ����kernel��ѯ
	uint8_t* resultsReturned = (uint8_t*)malloc(sizeof(uint8_t) * (this->trajNum + 1) * rangeNum);

	//timer.stop();
	//cout << "Time 2:" << timer.elapse() << "ms" << endl;

	//timer.start();
	cudaRangeQueryTestHandler(stateTableGPU, stateTableLength, resultsReturned, this->trajNum + 1, rangeNum, stream);
	//for (int jobID = 0; jobID <= rangeNum - 1; jobID++) {
	//	for (int traID = 0; traID <= this->trajNum; traID++) {
	//		if (resultsReturned[jobID*(this->trajNum + 1) + traID] == 1) {
	//			cout << "job " << jobID << "find" << traID << endl;
	//		}
	//	}
	//}
	//for (vector<uint8_t>::iterator iter = resultsReturned.begin(); iter != resultsReturned.end(); iter++) {
	//	//cout << (*iter) << endl;
	//	//printf("%d\n", *iter);
	//}
	//timer.stop();
	//cout << "Time 3:" << timer.elapse() << "ms" << endl;

	//FILE *fp = fopen("resultQuery.txt", "w+");
	//for (int i = 0; i <= stateTableLength - 1; i++) {
	//	for (int j = 0; j <= stateTableAllocate[i].candidatePointNum - 1; j++) {

	//		if ((resultsReturned[i*maxPointNum + j]) == (uint8_t)(1)) {
	//			fprintf(fp,"%d\n", stateTableAllocate[i].startIdxInAllPoints + j);
	//			fprintf(fp,"%f,%f\n", allPoints[stateTableAllocate[i].startIdxInAllPoints + j].x, allPoints[stateTableAllocate[i].startIdxInAllPoints + j].y);
	//		}

	//	}
	//}
	//��ѯ�������ƺ����stateTable�����gpu��
	CUDA_CALL(cudaFree(stateTableGPU));
	this->stateTableRange = stateTableAllocate;
	cudaStreamDestroy(stream);
	return 0;
}

int Grid::findMatchNodeInQuadTreeGPU(QuadtreeNode* node, MBB& bound, vector<QuadtreeNode*>* cells, cudaStream_t stream, int queryID)
{
	int totalLevel = int(log2(this->cellnum) / log2(4));
	if (node->isLeaf)
	{
		int startCellID = node->NodeID * int(pow(4, (totalLevel - node->level)));
		int startIdx = this->cellPtr[startCellID].pointRangeStart;
		int pointNum = node->numPoints;
		//���gpu�ڴ���û�и�node����Ϣ
		if (this->nodeAddrTable.find(startCellID) == this->nodeAddrTable.end())
		{
			CUDA_CALL(cudaMemcpyAsync(baseAddrGPU, &(this->allPoints[startIdx]), pointNum*sizeof(SPoint), cudaMemcpyHostToDevice, stream));
			this->stateTableRange->ptr = baseAddrGPU;
			this->nodeAddrTable.insert(pair<int, void*>(startCellID, baseAddrGPU));
			baseAddrGPU = (void*)((char*)baseAddrGPU + pointNum * sizeof(SPoint));
		}
		//����У����ٸ��ƣ�ֱ����
		else
		{
			this->stateTableRange->ptr = this->nodeAddrTable.find(startCellID)->second;
		}

		this->stateTableRange->xmin = bound.xmin;
		this->stateTableRange->xmax = bound.xmax;
		this->stateTableRange->ymin = bound.ymin;
		this->stateTableRange->ymax = bound.ymax;
		this->stateTableRange->candidatePointNum = pointNum;
		this->stateTableRange->startIdxInAllPoints = startIdx;
		this->stateTableRange->queryID = queryID;
		this->stateTableRange = this->stateTableRange + 1;
		this->stateTableLength = this->stateTableLength + 1;
	}
	else
	{
		if (bound.intersect(node->UL->mbb))
			findMatchNodeInQuadTreeGPU(node->UL, bound, cells, stream, queryID);
		if (bound.intersect(node->UR->mbb))
			findMatchNodeInQuadTreeGPU(node->UR, bound, cells, stream, queryID);
		if (bound.intersect(node->DL->mbb))
			findMatchNodeInQuadTreeGPU(node->DL, bound, cells, stream, queryID);
		if (bound.intersect(node->DR->mbb))
			findMatchNodeInQuadTreeGPU(node->DR, bound, cells, stream, queryID);
	}
	return 0;
}

int Grid::SimilarityQueryBatch(Trajectory* qTra, int queryTrajNum, int* topKSimilarityTraj, int kValue)
{
	MyTimer timer;
	//˼·���ֱ���ÿһ����ѯ�켣���ò�ͬstream����
	priority_queue<FDwithID, vector<FDwithID>, cmp>* queryQueue = new priority_queue<FDwithID, vector<FDwithID>, cmp>[queryTrajNum];
	map<int, int>* freqVectors = new map<int, int>[queryTrajNum];
	//Ϊ��ѯ����freqVector
	timer.start();
	for (int qID = 0; qID <= queryTrajNum - 1; qID++)
	{
		for (int pID = 0; pID <= qTra[qID].length - 1; pID++)
		{
			int cellid = WhichCellPointIn(SamplePoint(qTra[qID].points[pID].lon, qTra[qID].points[pID].lat, 1, 1));
			map<int, int>::iterator iter = freqVectors[qID].find(cellid);
			if (iter == freqVectors[qID].end())
			{
				freqVectors[qID].insert(pair<int, int>(cellid, 1));
			}
			else
			{
				freqVectors[qID][cellid] = freqVectors[qID][cellid] + 1;
			}
		}
	}
	timer.stop();
	cout << "Part1 time:" << timer.elapse() << endl;
	timer.start();
	//Ϊ��֦����Frequency Distance
	for (int qID = 0; qID <= queryTrajNum - 1; qID++)
	{
		this->freqVectors.formPriorityQueue(&queryQueue[qID], &freqVectors[qID]);
	}
	timer.stop();
	cout << "Part2 time:" << timer.elapse() << endl;
	//��һ�����ȶ��д洢��ǰ���Ž�����󶥶ѣ���֤��ʱ����pop����Ľ��
	priority_queue<FDwithID, vector<FDwithID>, cmpBig>* EDRCalculated = new priority_queue<FDwithID, vector<FDwithID>, cmpBig>[queryTrajNum];
	int* numElemInCalculatedQueue = new int[queryTrajNum]; //���浱ǰ���ȶ��н������֤���ȶ��д�С������kValue
	for (int i = 0; i <= queryTrajNum - 1; i++)
		numElemInCalculatedQueue[i] = 0;

	//׼����֮�󣬿�ʼ����ѯ
	const int k = KSIMILARITY;
	timer.start();
	// check if the FD is lowerbound for all traj



	// check if the FD is lowerbound for all traj
	for (int qID = 0; qID <= queryTrajNum - 1; qID++)
	{
		SPoint* queryTra = (SPoint*)malloc(sizeof(SPoint) * qTra[qID].length);
		for (int i = 0; i <= qTra[qID].length - 1; i++)
		{
			queryTra[i].x = qTra[qID].points[i].lon;
			queryTra[i].y = qTra[qID].points[i].lat;
			queryTra[i].tID = qTra[qID].tid;
		}
		int worstNow = 9999999;
		//timer.start();
		//printf("qID:%d", qID);
		/*MyTimer tt;*/
		while (worstNow > queryQueue[qID].top().FD)
		{
			/*tt.start();*/
			int candidateTrajID[k];
			//printf("%d", worstNow);
			//��ȡtopk
			for (int i = 0; i <= k - 1; i++)
			{
				candidateTrajID[i] = queryQueue[qID].top().traID;
				//printf("%d,%d\t", queryQueue[qID].top().traID,queryQueue[qID].top().FD);
				queryQueue[qID].pop();
			}
			//EDR calculate
			//��һ������AllPoints����ȡ�����켣
			SPoint** candidateTra = (SPoint**)malloc(sizeof(SPoint*) * k);
			int* candidateTraLength = (int*)malloc(sizeof(int) * k);
			for (int i = 0; i <= k - 1; i++)
			{
				candidateTra[i] = (SPoint*)malloc(sizeof(SPoint) * this->cellBasedTrajectory[candidateTrajID[i]].trajLength);
				SPoint* tempPtr = candidateTra[i];
				for (int subID = 0; subID <= this->cellBasedTrajectory[candidateTrajID[i]].length - 1; subID++)
				{
					int idxInAllPoints = this->cellBasedTrajectory[candidateTrajID[i]].startIdx[subID];
					memcpy(tempPtr, &this->allPoints[idxInAllPoints], sizeof(SPoint) * this->cellBasedTrajectory[candidateTrajID[i]].numOfPointInCell[subID]);
					//for (int cnt = 0; cnt <= this->cellBasedTrajectory[candidateTrajID[i]].numOfPointInCell[subID] - 1; cnt++) {
					//	candidateTra[i][cnt] = this->allPoints[idxInAllPoints+cnt];
					//}
					//printf("%d ", this->cellBasedTrajectory[candidateTrajID[i]].numOfPointInCell[subID]);
					tempPtr += this->cellBasedTrajectory[candidateTrajID[i]].numOfPointInCell[subID];
				}
				candidateTraLength[i] = this->cellBasedTrajectory[candidateTrajID[i]].trajLength;
			}
			//tt.stop();
			//cout << "Part3.1 time:" << tt.elapse() << endl;
			//tt.start();
			//�ڶ���������EDR
			//printf("%d", qID);
			int resultReturned[k];
			this->SimilarityExecuter(queryTra, candidateTra, qTra[qID].length, candidateTraLength, k, resultReturned);
			//tt.stop();
			//cout << "Part3.3 time:" << tt.elapse() << endl;
			//����worstNow
			for (int i = 0; i <= k - 1; i++)
			{
				if (numElemInCalculatedQueue[qID] < kValue)
				{
					//ֱ����PQ���
					FDwithID fd;
					fd.traID = candidateTrajID[i];
					fd.FD = resultReturned[i];
					EDRCalculated[qID].push(fd);
					numElemInCalculatedQueue[qID]++;
				}
				else
				{
					//��һ���Ƿ��PQ����ã�����ǵ���һ����ģ�����ȥһ���õģ����򲻶����ȶ���Ҳ������worstNow��
					int worstInPQ = EDRCalculated[qID].top().FD;
					if (resultReturned[i] < worstInPQ)
					{
						EDRCalculated[qID].pop();
						FDwithID fd;
						fd.traID = candidateTrajID[i];
						fd.FD = resultReturned[i];
						EDRCalculated[qID].push(fd);
					}
				}
			}
			worstNow = EDRCalculated[qID].top().FD;
			//printf("%d,worstNow:%d\t", qID,worstNow);
			//���ֽ������ͷ��ڴ�
			for (int i = 0; i <= k - 1; i++)
				free(candidateTra[i]);
			free(candidateTraLength);
			free(candidateTra);
		}
		//timer.stop();
		//cout << "Query Trajectory Length:" << qTra[qID].length << endl;
		//cout << "Part3 time:" << timer.elapse() << endl;
		//timer.start();
		free(queryTra);

		//timer.stop();
		//cout << "Part4 time:" << timer.elapse() << endl;
	}
	for (int qID = 0; qID <= queryTrajNum - 1;qID++)
	{
		for (int i = 0; i <= kValue - 1; i++)
		{
			topKSimilarityTraj[qID * kValue + i] = EDRCalculated[qID].top().traID;
			EDRCalculated[qID].pop();
		}
	}

	timer.stop();
	cout << "Part3 time:" << timer.elapse() << endl;

	delete[] EDRCalculated;
	delete[] numElemInCalculatedQueue;
	delete[] freqVectors;
	delete[] queryQueue;

	return 0;
}

int Grid::SimilarityMultiThreadHandler(priority_queue<FDwithID, vector<FDwithID>, cmp>* queryQueue, Trajectory* qTra, int queryTrajNum, priority_queue<FDwithID, vector<FDwithID>, cmpBig>* EDRCalculated, int kValue, int startQueryIdx)
{
	const int k = KSIMILARITY;
	int* numElemInCalculatedQueue = new int[queryTrajNum]; //���浱ǰ���ȶ��н������֤���ȶ��д�С������kValue
	for (int i = 0; i <= queryTrajNum - 1; i++)
		numElemInCalculatedQueue[i] = 0;
	for (int qID = startQueryIdx; qID <= startQueryIdx + queryTrajNum - 1; qID++)
	{
		SPoint* queryTra = (SPoint*)malloc(sizeof(SPoint) * qTra[qID].length);
		for (int i = 0; i <= qTra[qID].length - 1; i++)
		{
			queryTra[i].x = qTra[qID].points[i].lon;
			queryTra[i].y = qTra[qID].points[i].lat;
			queryTra[i].tID = qTra[qID].tid;
		}
		int worstNow = 9999999;
		//timer.start();
		//printf("qID:%d", qID);
		/*MyTimer tt;*/
		while (worstNow > queryQueue[qID].top().FD)
		{
			/*tt.start();*/
			int candidateTrajID[k];
			//printf("%d", worstNow);
			//��ȡtopk
			for (int i = 0; i <= k - 1; i++)
			{
				candidateTrajID[i] = queryQueue[qID].top().traID;
				//printf("%d,%d\t", queryQueue[qID].top().traID,queryQueue[qID].top().FD);
				queryQueue[qID].pop();
			}
			//EDR calculate
			//��һ������AllPoints����ȡ�����켣
			SPoint** candidateTra = (SPoint**)malloc(sizeof(SPoint*) * k);
			int* candidateTraLength = (int*)malloc(sizeof(int) * k);
			for (int i = 0; i <= k - 1; i++)
			{
				candidateTra[i] = (SPoint*)malloc(sizeof(SPoint) * this->cellBasedTrajectory[candidateTrajID[i]].trajLength);
				SPoint* tempPtr = candidateTra[i];
				for (int subID = 0; subID <= this->cellBasedTrajectory[candidateTrajID[i]].length - 1; subID++)
				{
					int idxInAllPoints = this->cellBasedTrajectory[candidateTrajID[i]].startIdx[subID];
					memcpy(tempPtr, &this->allPoints[idxInAllPoints], sizeof(SPoint) * this->cellBasedTrajectory[candidateTrajID[i]].numOfPointInCell[subID]);
					//for (int cnt = 0; cnt <= this->cellBasedTrajectory[candidateTrajID[i]].numOfPointInCell[subID] - 1; cnt++) {
					//	candidateTra[i][cnt] = this->allPoints[idxInAllPoints+cnt];
					//}
					//printf("%d ", this->cellBasedTrajectory[candidateTrajID[i]].numOfPointInCell[subID]);
					tempPtr += this->cellBasedTrajectory[candidateTrajID[i]].numOfPointInCell[subID];
				}
				candidateTraLength[i] = this->cellBasedTrajectory[candidateTrajID[i]].trajLength;
			}
			//tt.stop();
			//cout << "Part3.1 time:" << tt.elapse() << endl;
			//tt.start();
			//�ڶ���������EDR
			//printf("%d", qID);
			int resultReturned[k];
			this->SimilarityExecuter(queryTra, candidateTra, qTra[qID].length, candidateTraLength, k, resultReturned);
			//tt.stop();
			//cout << "Part3.3 time:" << tt.elapse() << endl;
			//����worstNow
			for (int i = 0; i <= k - 1; i++)
			{
				if (numElemInCalculatedQueue[qID-startQueryIdx] < kValue)
				{
					//ֱ����PQ���
					FDwithID fd;
					fd.traID = candidateTrajID[i];
					fd.FD = resultReturned[i];
					EDRCalculated[qID].push(fd);
					numElemInCalculatedQueue[qID-startQueryIdx]++;
				}
				else
				{
					//��һ���Ƿ��PQ����ã�����ǵ���һ����ģ�����ȥһ���õģ����򲻶����ȶ���Ҳ������worstNow��
					int worstInPQ = EDRCalculated[qID].top().FD;
					if (resultReturned[i] < worstInPQ)
					{
						EDRCalculated[qID].pop();
						FDwithID fd;
						fd.traID = candidateTrajID[i];
						fd.FD = resultReturned[i];
						EDRCalculated[qID].push(fd);
					}
				}
			}
			worstNow = EDRCalculated[qID].top().FD;
			//printf("%d,worstNow:%d\t", qID,worstNow);
			//���ֽ������ͷ��ڴ�
			for (int i = 0; i <= k - 1; i++)
				free(candidateTra[i]);
			free(candidateTraLength);
			free(candidateTra);
		}
		//timer.stop();
		//cout << "Query Trajectory Length:" << qTra[qID].length << endl;
		//cout << "Part3 time:" << timer.elapse() << endl;
		//timer.start();
		free(queryTra);

		//timer.stop();
		//cout << "Part4 time:" << timer.elapse() << endl;
	}
	return 0;
}

int Grid::FDCalculateParallelHandeler(priority_queue<FDwithID, vector<FDwithID>, cmp>* queue, map<int, int>* freqVectorQ)
{
	this->freqVectors.formPriorityQueue(queue, freqVectorQ);
	return 0;
}

int Grid::SimilarityQueryBatchCPUParallel(Trajectory* qTra, int queryTrajNum, int* topKSimilarityTraj, int kValue)
{
	MyTimer timer;
	//˼·���ò�ͬ�̴߳���ͬ������query
	priority_queue<FDwithID, vector<FDwithID>, cmp>* queryQueue = new priority_queue<FDwithID, vector<FDwithID>, cmp>[queryTrajNum];
	map<int, int>* freqVectors = new map<int, int>[queryTrajNum];
	//Ϊ��ѯ����freqVector
	timer.start();
	for (int qID = 0; qID <= queryTrajNum - 1; qID++)
	{
		for (int pID = 0; pID <= qTra[qID].length - 1; pID++)
		{
			int cellid = WhichCellPointIn(SamplePoint(qTra[qID].points[pID].lon, qTra[qID].points[pID].lat, 1, 1));
			map<int, int>::iterator iter = freqVectors[qID].find(cellid);
			if (iter == freqVectors[qID].end())
			{
				freqVectors[qID].insert(pair<int, int>(cellid, 1));
			}
			else
			{
				freqVectors[qID][cellid] = freqVectors[qID][cellid] + 1;
			}
		}
	}
	timer.stop();
	cout << "Part1 time:" << timer.elapse() << endl;
	timer.start();
	//Ϊ��֦����Frequency Distance
	vector<thread> threads_FD;
	for (int qID = 0; qID <= queryTrajNum - 1; qID++)
	{
		// this->freqVectors.formPriorityQueue(&queryQueue[qID], &freqVectors[qID]);
		threads_FD.push_back(thread(std::mem_fn(&Grid::FDCalculateParallelHandeler), this, &queryQueue[qID], &freqVectors[qID]));
	}
	std::for_each(threads_FD.begin(), threads_FD.end(), std::mem_fn(&std::thread::join));

	timer.stop();
	cout << "Part2 time:" << timer.elapse() << endl;
	//��һ�����ȶ��д洢��ǰ���Ž�����󶥶ѣ���֤��ʱ����pop����Ľ��
	priority_queue<FDwithID, vector<FDwithID>, cmpBig>* EDRCalculated = new priority_queue<FDwithID, vector<FDwithID>, cmpBig>[queryTrajNum];
	int* numElemInCalculatedQueue = new int[queryTrajNum]; //���浱ǰ���ȶ��н������֤���ȶ��д�С������kValue
	for (int i = 0; i <= queryTrajNum - 1; i++)
		numElemInCalculatedQueue[i] = 0;

	//׼����֮�󣬿�ʼ����ѯ
	const int k = KSIMILARITY;
	timer.start();
	// check if the FD is lowerbound for all traj

	const int THREAD_CPU = 4;
	vector<thread> threads;
	for (int i = 0; i <= queryTrajNum - 1;i++)
	{
		threads.push_back(thread(std::mem_fn(&Grid::SimilarityMultiThreadHandler), this, queryQueue, qTra, 1, EDRCalculated, kValue, i));
	}
	std::for_each(threads.begin(), threads.end(), std::mem_fn(&std::thread::join));



	//// check if the FD is lowerbound for all traj
	//for (int qID = 0; qID <= queryTrajNum - 1; qID++)
	//{
	//	SPoint* queryTra = (SPoint*)malloc(sizeof(SPoint) * qTra[qID].length);
	//	for (int i = 0; i <= qTra[qID].length - 1; i++)
	//	{
	//		queryTra[i].x = qTra[qID].points[i].lon;
	//		queryTra[i].y = qTra[qID].points[i].lat;
	//		queryTra[i].tID = qTra[qID].tid;
	//	}
	//	int worstNow = 9999999;
	//	//timer.start();
	//	//printf("qID:%d", qID);
	//	/*MyTimer tt;*/
	//	while (worstNow > queryQueue[qID].top().FD)
	//	{
	//		/*tt.start();*/
	//		int candidateTrajID[k];
	//		//printf("%d", worstNow);
	//		//��ȡtopk
	//		for (int i = 0; i <= k - 1; i++)
	//		{
	//			candidateTrajID[i] = queryQueue[qID].top().traID;
	//			//printf("%d,%d\t", queryQueue[qID].top().traID,queryQueue[qID].top().FD);
	//			queryQueue[qID].pop();
	//		}
	//		//EDR calculate
	//		//��һ������AllPoints����ȡ�����켣
	//		SPoint** candidateTra = (SPoint**)malloc(sizeof(SPoint*) * k);
	//		int* candidateTraLength = (int*)malloc(sizeof(int) * k);
	//		for (int i = 0; i <= k - 1; i++)
	//		{
	//			candidateTra[i] = (SPoint*)malloc(sizeof(SPoint) * this->cellBasedTrajectory[candidateTrajID[i]].trajLength);
	//			SPoint* tempPtr = candidateTra[i];
	//			for (int subID = 0; subID <= this->cellBasedTrajectory[candidateTrajID[i]].length - 1; subID++)
	//			{
	//				int idxInAllPoints = this->cellBasedTrajectory[candidateTrajID[i]].startIdx[subID];
	//				memcpy(tempPtr, &this->allPoints[idxInAllPoints], sizeof(SPoint) * this->cellBasedTrajectory[candidateTrajID[i]].numOfPointInCell[subID]);
	//				//for (int cnt = 0; cnt <= this->cellBasedTrajectory[candidateTrajID[i]].numOfPointInCell[subID] - 1; cnt++) {
	//				//	candidateTra[i][cnt] = this->allPoints[idxInAllPoints+cnt];
	//				//}
	//				//printf("%d ", this->cellBasedTrajectory[candidateTrajID[i]].numOfPointInCell[subID]);
	//				tempPtr += this->cellBasedTrajectory[candidateTrajID[i]].numOfPointInCell[subID];
	//			}
	//			candidateTraLength[i] = this->cellBasedTrajectory[candidateTrajID[i]].trajLength;
	//		}
	//		//tt.stop();
	//		//cout << "Part3.1 time:" << tt.elapse() << endl;
	//		//tt.start();
	//		//�ڶ���������EDR
	//		//printf("%d", qID);
	//		int resultReturned[k];
	//		this->SimilarityExecuter(queryTra, candidateTra, qTra[qID].length, candidateTraLength, k, resultReturned);
	//		//tt.stop();
	//		//cout << "Part3.3 time:" << tt.elapse() << endl;
	//		//����worstNow
	//		for (int i = 0; i <= k - 1; i++)
	//		{
	//			if (numElemInCalculatedQueue[qID] < kValue)
	//			{
	//				//ֱ����PQ���
	//				FDwithID fd;
	//				fd.traID = candidateTrajID[i];
	//				fd.FD = resultReturned[i];
	//				EDRCalculated[qID].push(fd);
	//				numElemInCalculatedQueue[qID]++;
	//			}
	//			else
	//			{
	//				//��һ���Ƿ��PQ����ã�����ǵ���һ����ģ�����ȥһ���õģ����򲻶����ȶ���Ҳ������worstNow��
	//				int worstInPQ = EDRCalculated[qID].top().FD;
	//				if (resultReturned[i] < worstInPQ)
	//				{
	//					EDRCalculated[qID].pop();
	//					FDwithID fd;
	//					fd.traID = candidateTrajID[i];
	//					fd.FD = resultReturned[i];
	//					EDRCalculated[qID].push(fd);
	//				}
	//			}
	//		}
	//		worstNow = EDRCalculated[qID].top().FD;
	//		//printf("%d,worstNow:%d\t", qID,worstNow);
	//		//���ֽ������ͷ��ڴ�
	//		for (int i = 0; i <= k - 1; i++)
	//			free(candidateTra[i]);
	//		free(candidateTraLength);
	//		free(candidateTra);
	//	}
	//	//timer.stop();
	//	//cout << "Query Trajectory Length:" << qTra[qID].length << endl;
	//	//cout << "Part3 time:" << timer.elapse() << endl;
	//	//timer.start();
	//	free(queryTra);

	//	//timer.stop();
	//	//cout << "Part4 time:" << timer.elapse() << endl;
	//}	 

	for (int qID = 0; qID <= queryTrajNum - 1; qID++)
	{
		for (int i = 0; i <= kValue - 1; i++)
		{
			topKSimilarityTraj[qID * kValue + i] = EDRCalculated[qID].top().traID;
			EDRCalculated[qID].pop();
		}
	}

	timer.stop();
	cout << "Part3 time:" << timer.elapse() << endl;

	delete[] EDRCalculated;
	delete[] numElemInCalculatedQueue;
	delete[] freqVectors;
	delete[] queryQueue;

	return 0;
}

int Grid::SimilarityExecuter(SPoint* queryTra, SPoint** candidateTra, int queryLength, int* candidateLength, int candSize, int* resultArray)
{
	for (int i = 0; i <= candSize - 1; i++)
	{
		//ÿ��DP����
		SPoint *CPUqueryTra = queryTra, *CPUCandTra = candidateTra[i];
		int CPUqueryLength = queryLength, CPUCandLength = candidateLength[i];
		int longest = 0;

		const SPoint *tra1, *tra2;
		int len1, len2;
		//printf("%d,%d\t", len1, len2);
		if (CPUCandLength >= CPUqueryLength)
		{
			tra1 = CPUqueryTra;
			tra2 = CPUCandTra;
			len1 = CPUqueryLength;
			len2 = CPUCandLength;
		}
		else
		{
			tra1 = CPUCandTra;
			tra2 = CPUqueryTra;
			len1 = CPUCandLength;
			len2 = CPUqueryLength;
		}

		if (CPUqueryLength >= longest)
		{
			longest = CPUqueryLength;
		}
		else
		{
			longest = CPUCandLength;
		}


		int** stateTable = (int**)malloc(sizeof(int*) * (len1 + 1));
		for (int j = 0; j <= len1; j++)
		{
			stateTable[j] = (int*)malloc(sizeof(int) * (len2 + 1));
		}
		stateTable[0][0] = 0;
		for (int row = 1; row <= len1; row++)
		{
			stateTable[row][0] = row;
		}
		for (int col = 1; col <= len2; col++)
		{
			stateTable[0][col] = col;
		}

		for (int row = 1; row <= len1; row++)
		{
			for (int col = 1; col <= len2; col++)
			{
				SPoint p1 = tra1[row - 1];
				SPoint p2 = tra2[col - 1]; //�������ڴ��Ǿۼ����ʵ���
				bool subcost;
				if (((p1.x - p2.x) * (p1.x - p2.x) + (p1.y - p2.y) * (p1.y - p2.y)) < EPSILON)
				{
					subcost = 0;
				}
				else
					subcost = 1;
				int myState = 0;
				int state_ismatch = stateTable[row - 1][col - 1] + subcost;
				int state_up = stateTable[row - 1][col] + 1;
				int state_left = stateTable[row][col - 1] + 1;
				//if (state_ismatch < state_up)
				//	myState = state_ismatch;
				//else if (state_left < state_up)
				//	myState = state_left;
				//else
				//	myState = state_up;
				bool c1 = ((state_ismatch < state_up) && (state_ismatch < state_left));
				bool c2 = ((state_left < state_up) && ((state_left < state_ismatch)));
				//ȥ��if�ı�﷽ʽ���Ƿ�����������ܣ�
				myState = c1 * state_ismatch + c2 * state_left + !(c1 || c2) * state_up;

				stateTable[row][col] = myState;
				//	if (row == len1&&col == len2)
				//cout << myState << endl;
			}
		}


		resultArray[i] = stateTable[len1][len2];
		//cout << resultCPU[i] << endl;
		for (int j = 0; j <= len1; j++)
		{
			free(stateTable[j]);
		}
		free(stateTable);
	}
	return 0;
}


int Grid::SimilarityQueryBatchOnGPU(Trajectory* qTra, int queryTrajNum, int* topKSimilarityTraj, int kValue)
//���У�������query�����ϵ�GPU����
//����˼·���ֱ���ÿһ����ѯ�켣���ò�ͬstream����
{
	CUDA_CALL(cudaMalloc((void**)(&baseAddrGPU), 768 * 1024 * 1024));
	void* whileAddrGPU = NULL;
	CUDA_CALL(cudaMalloc((void**)(&whileAddrGPU), 256 * 1024 * 1024));
	void* whileAddrGPUBase = whileAddrGPU;
	//��ǰ���䵽�ĵ�ַ
	void* nowAddrGPU = NULL;
	cudaStream_t defaultStream;
	cudaStreamCreate(&defaultStream);

	MyTimer timer;

	priority_queue<FDwithID, vector<FDwithID>, cmp>* queryQueue = new priority_queue<FDwithID, vector<FDwithID>, cmp>[queryTrajNum];
	map<int, int>* freqVectors = new map<int, int>[queryTrajNum];
	//Ϊ��ѯ����freqVector
	timer.start();
	for (int qID = 0; qID <= queryTrajNum - 1; qID++)
	{
		for (int pID = 0; pID <= qTra[qID].length - 1; pID++)
		{
			int cellid = WhichCellPointIn(SamplePoint(qTra[qID].points[pID].lon, qTra[qID].points[pID].lat, 1, 1));
			map<int, int>::iterator iter = freqVectors[qID].find(cellid);
			if (iter == freqVectors[qID].end())
			{
				freqVectors[qID].insert(pair<int, int>(cellid, 1));
			}
			else
			{
				freqVectors[qID][cellid] = freqVectors[qID][cellid] + 1;
			}
		}
	}
	timer.stop();
	cout << "Part1 time:" << timer.elapse() << endl;
	timer.start();
	//Ϊ��֦����Frequency Distance
	for (int qID = 0; qID <= queryTrajNum - 1; qID++)
	{
		this->freqVectors.formPriorityQueue(&queryQueue[qID], &freqVectors[qID]);
		//this->freqVectors.formPriorityQueueGPU(&queryQueue[qID], &freqVectors[qID]);
	}
	timer.stop();
	cout << "Part2 time:" << timer.elapse() << endl;
	//��һ�����ȶ��д洢��ǰ���Ž�����󶥶ѣ���֤��ʱ����pop����Ľ��
	timer.start();
	//MyTimer tt;
	//tt.start();
	priority_queue<FDwithID, vector<FDwithID>, cmpBig>* EDRCalculated = new priority_queue<FDwithID, vector<FDwithID>, cmpBig>[queryTrajNum];
	int* numElemInCalculatedQueue = new int[queryTrajNum]; //���浱ǰ���ȶ��н������֤���ȶ��д�С������kValue
	for (int i = 0; i <= queryTrajNum - 1; i++)
		numElemInCalculatedQueue[i] = 0;

	//׼����֮�󣬿�ʼ����ѯ
	const int k = KSIMILARITY;
	int totalQueryTrajLength = 0;
	for (int qID = 0; qID <= queryTrajNum - 1; qID++)
	{
		totalQueryTrajLength += qTra[qID].length;
	}
	//��ѯ�켣��Ϣ��׼����
	//�����ѯ�Ĺ켣
	SPoint* allQueryTra = (SPoint*)malloc(sizeof(SPoint) * totalQueryTrajLength);
	//������allQueryTra�и����켣��offset����ʼ��ַ��
	int* allQueryTraOffset = new int[queryTrajNum];
	SPoint* queryTra = allQueryTra;
	SPoint* queryTraGPU = (SPoint*)baseAddrGPU;
	//������Ǳ�������queryTra�Ļ�ַ
	SPoint* queryTraGPUBase = queryTraGPU;
	int* queryTraLength = new int[queryTrajNum];
	allQueryTraOffset[0] = 0;
	printf("queryTrajNum:%d", queryTrajNum);
	printf("totalQueryTrajLength:%d", totalQueryTrajLength);
	for (int qID = 0; qID <= queryTrajNum - 1; qID++)
	{
		for (int i = 0; i <= qTra[qID].length - 1; i++)
		{
			queryTra[i].x = qTra[qID].points[i].lon;
			queryTra[i].y = qTra[qID].points[i].lat;
			queryTra[i].tID = qTra[qID].tid;
		}
		CUDA_CALL(cudaMemcpyAsync(queryTraGPU, queryTra, sizeof(SPoint)*qTra[qID].length, cudaMemcpyHostToDevice, defaultStream));
		queryTraLength[qID] = qTra[qID].length;
		queryTraGPU = queryTraGPU + qTra[qID].length;
		queryTra += qTra[qID].length;
		if (qID != queryTrajNum - 1)
			allQueryTraOffset[qID + 1] = allQueryTraOffset[qID] + qTra[qID].length;
	}
	nowAddrGPU = queryTraGPU;
	// queryTraOffsetGPU�Ǳ���queryTra��offset�Ļ���ַ
	int* queryTraOffsetGPU = (int*)nowAddrGPU;
	CUDA_CALL(cudaMemcpyAsync(queryTraOffsetGPU, allQueryTraOffset, sizeof(int)*queryTrajNum, cudaMemcpyHostToDevice, defaultStream));
	nowAddrGPU = (void*)((int*)nowAddrGPU + queryTrajNum);

	//����queryLength
	int* queryLengthGPU = (int*)nowAddrGPU;
	CUDA_CALL(cudaMemcpyAsync(queryLengthGPU, queryTraLength, sizeof(int)*queryTrajNum, cudaMemcpyHostToDevice, defaultStream));
	nowAddrGPU = (void*)((int*)nowAddrGPU + queryTrajNum);
	//tt.stop();
	//cout << "Part3.0.1 time:" << tt.elapse() << endl;
	//tt.start();
	//��һ����ѭ������֦
	int* worstNow = new int[queryTrajNum];
	for (int qID = 0; qID <= queryTrajNum - 1; qID++)
	{
		worstNow[qID] = 9999999;
	}
	int* candidateTraLength = (int*)malloc(sizeof(int) * k * queryTrajNum);
	bool* isFinished = new bool[queryTrajNum];
	for (int qID = 0; qID <= queryTrajNum - 1; qID++)
	{
		isFinished[qID] = FALSE;
	}
	bool isAllFinished = FALSE;
	SPoint* candidateTra = (SPoint*)malloc(sizeof(SPoint) * k * queryTrajNum * MAXLENGTH);
	int** candidateTrajID = new int*[queryTrajNum];
	for (int i = 0; i <= queryTrajNum - 1; i++)
		candidateTrajID[i] = new int[k];
	//����qid��candID����candidateTran�е�offset�Ķ�Ӧ��ϵ
	TaskInfoTableForSimilarity* taskInfoTable = (TaskInfoTableForSimilarity *)malloc(sizeof(TaskInfoTableForSimilarity) * k * queryTrajNum);
	//�켣Ψһ��������id�¹켣��id��baseAddr ���б�Ҫ����켣��id�𣿣�
	OffsetTable* candidateTrajOffsetTable = (OffsetTable*)malloc(sizeof(OffsetTable) * k * queryTrajNum);
	//����candidateOffset�Ļ���ַ��
	SPoint** candidateOffsets = (SPoint**)malloc(sizeof(SPoint*) * k * queryTrajNum);
	int candidateTrajNum = 0;
	// traID����candidateTrajOffsetTable�е�idx�Ķ�Ӧ��ϵmap����Ҫ�����жϹ켣�Ƿ��Ѿ����Ƶ�gpu
	map<int, void*> traID_baseAddr;
	SPoint* candidateTraGPU = (SPoint*)nowAddrGPU;
	//tt.stop();
	//cout << "Part3.0.2 time:" << tt.elapse() << endl;
	
	while (!isAllFinished)
	{
		//tt.start();
		//���д�����ĺ�ѡ�켣������Ŀ���൱��������Ŀ
		int validCandTrajNum = 0;
		//Ŀǰ��û�м�����ɵĲ�ѯ�켣����Ŀ
		int validQueryTraNum = queryTrajNum;
		// �ڱ��ּ����ڵĵڼ����켣
		int validQueryIdx = 0;
		// ���Ե�ַ
		SPoint* tempPtr = candidateTra;
		for (int qID = 0; qID <= queryTrajNum - 1; qID++)
		{
			
			if (isFinished[qID])
				validQueryTraNum--;
			if (!isFinished[qID])
			{
				if ((queryQueue[qID].empty()) || (worstNow[qID] <= queryQueue[qID].top().FD))
				{
					validQueryTraNum--;
					isFinished[qID] = TRUE;
					continue;
				}
				else
				{
					//��ȡtopk��Ϊ����켣
					for (int i = 0; i <= k - 1; i++)
					{
						candidateTrajID[qID][i] = queryQueue[qID].top().traID;
						//printf("%d,%d ", queryQueue[qID].top().traID, queryQueue[qID].top().FD);
						//printf("%d,%d\t", queryQueue[qID].top().traID,queryQueue[qID].top().FD);
						//if ((qID == 27) && queryQueue[qID].size()==10)
						//	printf("%d..", queryQueue[qID].size());
						queryQueue[qID].pop();
						validCandTrajNum++;
					}
					for (int i = 0; i <= k - 1; i++)
					{
						int CandTrajID = candidateTrajID[qID][i];
						map<int, void*>::iterator traID_baseAddr_ITER = traID_baseAddr.find(CandTrajID);
						// ����켣��û�б�����GPU��
						if (traID_baseAddr_ITER == traID_baseAddr.end())
						{
							int pointsNumInThisCand = 0;
							SPoint* thisTrajAddr = tempPtr;
							for (int subID = 0; subID <= this->cellBasedTrajectory[candidateTrajID[qID][i]].length - 1; subID++)
							{
								int idxInAllPoints = this->cellBasedTrajectory[candidateTrajID[qID][i]].startIdx[subID];
								memcpy(tempPtr, &this->allPoints[idxInAllPoints], sizeof(SPoint) * this->cellBasedTrajectory[candidateTrajID[qID][i]].numOfPointInCell[subID]);
								tempPtr += this->cellBasedTrajectory[candidateTrajID[qID][i]].numOfPointInCell[subID];
								pointsNumInThisCand += this->cellBasedTrajectory[candidateTrajID[qID][i]].numOfPointInCell[subID];
							}
							// �������켣��ȡ��candidateTraGPU��
							CUDA_CALL(cudaMemcpyAsync(candidateTraGPU, thisTrajAddr, pointsNumInThisCand*sizeof(SPoint), cudaMemcpyHostToDevice, defaultStream));
							traID_baseAddr[candidateTrajID[qID][i]] = candidateTraGPU;
							//������Ҫ�����query��candidateTraLength
							candidateTraLength[k * validQueryIdx + i] = this->cellBasedTrajectory[candidateTrajID[qID][i]].trajLength;
							//������Ҫ�����query��offset
							taskInfoTable[k * validQueryIdx + i].qID = qID;
							taskInfoTable[k * validQueryIdx + i].candTrajID = CandTrajID;
							// ����켣��Ӧ��addr
							candidateTrajOffsetTable[k * validQueryIdx + i].objectId = candidateTrajID[qID][i];
							candidateTrajOffsetTable[k * validQueryIdx + i].addr = candidateTraGPU;
							candidateOffsets[k * validQueryIdx + i] = candidateTraGPU;
							//��ַ��ǰ�ƶ���������һ�θ���
							candidateTraGPU = (candidateTraGPU + pointsNumInThisCand);
							// nowAddrGPU ʼ����ָ����һ�����е�GPU��ַ
							nowAddrGPU = (void*)candidateTraGPU;
						}
						// ����ù켣�Ѿ����ƽ���gpu���棬��ôֻ��Ҫ���ոù켣id���±������
						else
						{
							void* baseAddrGPU = traID_baseAddr_ITER->second;
							//������Ҫ�����query��candidateTraLength
							candidateTraLength[k * validQueryIdx + i] = this->cellBasedTrajectory[CandTrajID].trajLength;
							//������Ҫ�����query��offset
							taskInfoTable[k * validQueryIdx + i].qID = qID;
							taskInfoTable[k * validQueryIdx + i].candTrajID = CandTrajID;
							// ����켣��Ӧ��addr
							candidateTrajOffsetTable[k * validQueryIdx + i].objectId = CandTrajID;
							candidateTrajOffsetTable[k * validQueryIdx + i].addr = baseAddrGPU;
							candidateOffsets[k * validQueryIdx + i] = (SPoint*)baseAddrGPU;
						}
					}
					validQueryIdx++;
					//���գ���Ҫ�������EDR��validQueryIdx * k ��
					// �����ȷ��validQueryIdx * k Ӧ������validCandTrajNum
					//�����ȷ��validQueryIdxӦ������validQueryNum
				}
			}
		}
		//tt.stop();
		//cout << "Part3.1 time:" << tt.elapse() << endl;
		//tt.start();
		//����candidateTraj��ɣ�����candidateTrajLength
		int* candidateTraLengthGPU = (int*)whileAddrGPU;
		CUDA_CALL(cudaMemcpyAsync(candidateTraLengthGPU, candidateTraLength, sizeof(int)*validCandTrajNum, cudaMemcpyHostToDevice, defaultStream));
		// nowAddrGPU ʼ����ָ����һ�����е�GPU��ַ
		whileAddrGPU = (void*)((int*)whileAddrGPU + validCandTrajNum);

		//����TaskInfoTable
		TaskInfoTableForSimilarity* taskInfoTableGPU = (TaskInfoTableForSimilarity*)whileAddrGPU;
		CUDA_CALL(cudaMemcpyAsync(taskInfoTableGPU, taskInfoTable, sizeof(TaskInfoTableForSimilarity)*validCandTrajNum, cudaMemcpyHostToDevice, defaultStream));
		// nowAddrGPU ʼ����ָ����һ�����е�GPU��ַ
		whileAddrGPU = (void*)((TaskInfoTableForSimilarity*)whileAddrGPU + validCandTrajNum);

		//����candidate�ĵ�ַ��gpu��
		SPoint** candidateOffsetsGPU = (SPoint**)whileAddrGPU;
		CUDA_CALL(cudaMemcpyAsync(candidateOffsetsGPU, candidateOffsets, sizeof(SPoint*)*validCandTrajNum, cudaMemcpyHostToDevice, defaultStream));
		// nowAddrGPU ʼ����ָ����һ�����е�GPU��ַ
		whileAddrGPU = (void*)((SPoint**)whileAddrGPU + validCandTrajNum);

		//����candidateTraj��candidateLength��ɣ�׼������Similarity search
		//ֻ��Ҫ��ѯisFinishedΪfalse��queryTra����������Щ����ֱ�ӿ�offsetTableCandidateTra
		int *resultReturned = new int[queryTrajNum*k];
		int* resultReturnedGPU = (int*)whileAddrGPU;
		whileAddrGPU = (void*)((int*)whileAddrGPU + k*queryTrajNum);
	
		//CUDA_CALL(cudaMalloc((void**)resultReturnedGPU, sizeof(int)*k*queryTrajNum));



		//tt.stop();
		//cout << "Part3.2 time:" << tt.elapse() << endl;
		//tt.start();
		//�������ķ���û�д���ʼ����EDR
		if (validQueryTraNum * k == validCandTrajNum)
		{
			EDRDistance_Batch_Handler(validCandTrajNum, taskInfoTableGPU, queryTraGPUBase, queryTraOffsetGPU, candidateOffsetsGPU, queryLengthGPU, candidateTraLengthGPU, resultReturnedGPU, &defaultStream);
			CUDA_CALL(cudaMemcpyAsync(resultReturned, resultReturnedGPU, sizeof(int)*k*queryTrajNum, cudaMemcpyDeviceToHost, defaultStream));
		}
		else
		{
			printf("error in line 1007\n");
		}

		//tt.stop();
		//cout << "Part3.3 time:" << tt.elapse() << endl;
		//tt.start();
		//���м�������󣬸���worstNow�Լ�д����
		for (int idx = 0; idx <= k * validQueryTraNum - 1; idx++)
		{
			int qID = taskInfoTable[idx].qID;
			int i = idx % k;
			if (numElemInCalculatedQueue[qID] < kValue)
			{
				//ֱ����PQ���
				FDwithID fd;
				fd.traID = candidateTrajID[qID][i];
				fd.FD = resultReturned[idx];
				EDRCalculated[qID].push(fd);
				numElemInCalculatedQueue[qID]++;
			}
			else
			{
				//��һ���Ƿ��PQ����ã�����ǵ���һ����ģ�����ȥһ���õģ����򲻶����ȶ���Ҳ������worstNow��
				int worstInPQ = EDRCalculated[qID].top().FD;
				if (resultReturned[i] < worstInPQ)
				{
					EDRCalculated[qID].pop();
					FDwithID fd;
					fd.traID = candidateTrajID[qID][i];
					fd.FD = resultReturned[idx];
					EDRCalculated[qID].push(fd);
				}
			}
		}
		for (int qID = 0; qID <= queryTrajNum - 1; qID++)
			worstNow[qID] = EDRCalculated[qID].top().FD;

		
		bool temp = TRUE;
		for (int qID = 0; qID <= queryTrajNum - 1; qID++)
		{
			temp = temp && isFinished[qID];
		}
		isAllFinished = temp;
		delete[] resultReturned;
		//GPUָ��ص�while��ʼ�ĵط�
		whileAddrGPU = whileAddrGPUBase;
		//tt.stop();
		//cout << "Part3.4 time:" << tt.elapse() << endl;
	}


	/*
	for (int qID = 0; qID <= queryTrajNum - 1; qID++) {

		timer.start();
		int candidateTrajID[k];
		//printf("qID:%d", qID);
		while (worstNow[qID] > queryQueue[qID].top().FD) {
			//printf("%d", worstNow);
			//��ȡtopk
			for (int i = 0; i <= k - 1; i++) {
				candidateTrajID[i] = queryQueue[qID].top().traID;
				//printf("%d,%d\t", queryQueue[qID].top().traID,queryQueue[qID].top().FD);
				queryQueue[qID].pop();
			}
			//EDR calculate
			//��һ������AllPoints����ȡ�����켣
			SPoint **candidateTra = (SPoint**)malloc(sizeof(SPoint*)*k);

			for (int i = 0; i <= k - 1; i++) {
				candidateTra[i] = (SPoint*)malloc(sizeof(SPoint)*this->cellBasedTrajectory[candidateTrajID[i]].trajLength);
				SPoint *tempPtr = candidateTra[i];
				for (int subID = 0; subID <= this->cellBasedTrajectory[candidateTrajID[i]].length - 1; subID++) {
					int idxInAllPoints = this->cellBasedTrajectory[candidateTrajID[i]].startIdx[subID];
					memcpy(tempPtr, &this->allPoints[idxInAllPoints], sizeof(SPoint)*this->cellBasedTrajectory[candidateTrajID[i]].numOfPointInCell[subID]);
					//for (int cnt = 0; cnt <= this->cellBasedTrajectory[candidateTrajID[i]].numOfPointInCell[subID] - 1; cnt++) {
					//	candidateTra[i][cnt] = this->allPoints[idxInAllPoints+cnt];
					//}
					//printf("%d ", this->cellBasedTrajectory[candidateTrajID[i]].numOfPointInCell[subID]);
					tempPtr += this->cellBasedTrajectory[candidateTrajID[i]].numOfPointInCell[subID];
				}
				candidateTraLength[i] = this->cellBasedTrajectory[candidateTrajID[i]].trajLength;
			}
			//�ڶ���������EDR
			int resultReturned[k];
			this->SimilarityExecuter(queryTra, candidateTra, qTra[qID].length, candidateTraLength, k, resultReturned);
			//����worstNow
			for (int i = 0; i <= k - 1; i++) {
				if (numElemInCalculatedQueue[qID] < kValue) {
					//ֱ����PQ���
					FDwithID fd;
					fd.traID = candidateTrajID[i];
					fd.FD = resultReturned[i];
					EDRCalculated[qID].push(fd);
					numElemInCalculatedQueue[qID]++;
				}
				else {
					//��һ���Ƿ��PQ����ã�����ǵ���һ����ģ�����ȥһ���õģ����򲻶����ȶ���Ҳ������worstNow��
					int worstInPQ = EDRCalculated[qID].top().FD;
					if (resultReturned[i] < worstInPQ) {
						EDRCalculated[qID].pop();
						FDwithID fd;
						fd.traID = candidateTrajID[i];
						fd.FD = resultReturned[i];
						EDRCalculated[qID].push(fd);
					}
				}
			}
			worstNow = EDRCalculated[qID].top().FD;
			//printf("%d,worstNow:%d\t", qID,worstNow);
			//���ֽ������ͷ��ڴ�
			for (int i = 0; i <= k - 1; i++)
				free(candidateTra[i]);
			free(candidateTraLength);
			free(candidateTra);

		}
		timer.stop();
		cout << "Query Trajectory Length:" << qTra[qID].length << endl;
		cout << "Part3 time:" << timer.elapse() << endl;
		timer.start();
		free(queryTra);
		for (int i = 0; i <= kValue - 1; i++) {
			topKSimilarityTraj[qID*kValue + i] = EDRCalculated[qID].top().traID;
			EDRCalculated[qID].pop();
		}
		timer.stop();
		cout << "Part4 time:" << timer.elapse() << endl;
	}
	*/

	timer.stop();
	cout << "Part3 time:" << timer.elapse() << endl;

	//������
	for (int qID = 0; qID <= queryTrajNum - 1; qID++) {
		for (int i = 0; i <= kValue - 1; i++)
		{
			topKSimilarityTraj[qID * kValue + i] = EDRCalculated[qID].top().traID;
			EDRCalculated[qID].pop();
		}
	}

	for (int i = 0; i <= queryTrajNum - 1; i++)
		delete[] candidateTrajID[i];
	free(taskInfoTable);
	free(candidateTrajOffsetTable);
	free(candidateOffsets);
	delete[] candidateTrajID;
	free(candidateTra);
	delete[] isFinished;
	free(candidateTraLength);
	delete[] worstNow;
	free(allQueryTra);
	delete[] allQueryTraOffset;
	delete[] EDRCalculated;
	delete[] numElemInCalculatedQueue;
	delete[] freqVectors;
	delete[] queryQueue;
	delete[] queryTraLength;
	CUDA_CALL(cudaFree(baseAddrGPU));
	cudaStreamDestroy(defaultStream);
	return 0;
}

//
//int Grid::SimilarityQuery(Trajectory & qTra, Trajectory **candTra, const int candSize, float * EDRdistance)
//{
//	cout << candSize << endl;
//	SPoint *queryTra = (SPoint*)malloc(sizeof(SPoint)*(qTra.length));
//	for (int i = 0; i <= qTra.length - 1; i++) {
//		queryTra[i].x = qTra.points[i].lon;
//		queryTra[i].y = qTra.points[i].lat;
//		queryTra[i].tID = qTra.points[i].tid;
//	}
//
//	SPoint **candidateTra = (SPoint**)malloc(sizeof(SPoint*)*candSize);
//
//	for (int i = 0; i <= candSize - 1; i++) {
//		candidateTra[i] = (SPoint*)malloc(sizeof(SPoint)*(candTra[i]->length)); //���Ե�ʱ����һ�����ܱ��ڴ����FFFFF
//		for (int j = 0; j <= candTra[i]->length - 1; j++) {
//			candidateTra[i][j].x = candTra[i]->points[j].lon;
//			candidateTra[i][j].y = candTra[i]->points[j].lat;
//			candidateTra[i][j].tID = candTra[i]->points[j].tid;
//		}
//	}
//
//	int queryLength = qTra.length;
//	int *candidateLength = (int*)malloc(sizeof(int)*candSize);
//	for (int i = 0; i <= candSize - 1; i++) {
//		candidateLength[i] = candTra[i]->length;
//	}
//
//	int* result = (int*)malloc(sizeof(int)*candSize);
//
//	MyTimer timer1;
//	timer1.start();
//
//	//CPU
//	int *resultCPU = (int*)malloc(sizeof(int)*candSize);
//	for (int i = 0; i <= candSize - 1; i++) {
//		//ÿ��DP����
//		SPoint *CPUqueryTra = queryTra, *CPUCandTra = candidateTra[i];
//		int CPUqueryLength = qTra.length, CPUCandLength = candidateLength[i];
//		int longest = 0;
//
//		const SPoint *tra1, *tra2;
//		int len1, len2;
//		if (CPUCandLength >= CPUqueryLength) {
//			tra1 = CPUqueryTra;
//			tra2 = CPUCandTra;
//			len1 = CPUqueryLength;
//			len2 = CPUCandLength;
//		}
//		else
//		{
//			tra1 = CPUCandTra;
//			tra2 = CPUqueryTra;
//			len1 = CPUCandLength;
//			len2 = CPUqueryLength;
//		}
//
//		if (CPUqueryLength >= longest) {
//			longest = CPUqueryLength;
//		}
//		else
//		{
//			longest = CPUCandLength;
//		}
//
//
//		int **stateTable = (int**)malloc(sizeof(int*)*(len1 + 1));
//		for (int j = 0; j <= len1; j++) {
//			stateTable[j] = (int*)malloc(sizeof(int)*(len2 + 1));
//		}
//		stateTable[0][0] = 0;
//		for (int row = 1; row <= len1; row++) {
//			stateTable[row][0] = row;
//		}
//		for (int col = 1; col <= len2; col++) {
//			stateTable[0][col] = col;
//		}
//
//		for (int row = 1; row <= len1; row++) {
//			for (int col = 1; col <= len2; col++) {
//				SPoint p1 = tra1[row - 1];
//				SPoint p2 = tra2[col - 1]; //�������ڴ��Ǿۼ����ʵ���
//				bool subcost;
//				if (((p1.x - p2.x)*(p1.x - p2.x) + (p1.y - p2.y)*(p1.y - p2.y)) < EPSILON) {
//					subcost = 0;
//				}
//				else
//					subcost = 1;
//				int myState = 0;
//				int state_ismatch = stateTable[row - 1][col - 1] + subcost;
//				int state_up = stateTable[row - 1][col] + 1;
//				int state_left = stateTable[row][col - 1] + 1;
//				if (state_ismatch < state_up)
//					myState = state_ismatch;
//				else if (state_left < state_up)
//					myState = state_left;
//				else
//					myState = state_ismatch;
//
//				stateTable[row][col] = myState;
//				//	if (row == len1&&col == len2)
//						//cout << myState << endl;
//			}
//		}
//
//		resultCPU[i] = stateTable[len1][len2];
//		//cout << resultCPU[i] << endl;
//	}
//	timer1.stop();
//	cout << "CPU Similarity Time:" << timer1.elapse() << "ms" << endl;
//	//GPU
//
//	timer1.start();
//	handleEDRdistance(queryTra, candidateTra, candSize, queryLength, candidateLength, result);
//	timer1.stop();
//	cout << "GPU Similarity Time:" << timer1.elapse() << "ms" << endl;
//
//	for (int i = 0; i <= candSize - 1; i++) {
//		EDRdistance[i] = result[i];
//	}
//	free(queryTra);
//	for (int i = 0; i <= candSize - 1; i++) {
//		free(candidateTra[i]);
//	}
//	free(candidateTra);
//	free(candidateLength);
//	free(result);
//
//	return 0;
//}


Grid::~Grid()
{
}
