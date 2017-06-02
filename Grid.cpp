#include "Grid.h"
#include "cudaKernel.h"
#include "WinTimer.h"

extern Trajectory* tradb;
MyTimer timer;

Grid::Grid()
{
	range = MBB(0, 0, 0, 0);
	cellnum = 0;
	cell_size = 0;
	cellNum_axis = 0;
	cellPtr = NULL;
	QuadtreeNode *root;
#ifdef _CELL_BASED_STORAGE
	allPoints = NULL;
	allPointsPtrGPU = NULL;
#endif // _CELL_BASED_STORAGE

}

//���Թ���û����
int Grid::getIdxFromXY(int x, int y)
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
	for (int i = 1; i <= 2 * max(lenx, leny); i++) {
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

int Grid::buildQuadTree(int level, int id, QuadtreeNode* pNode, QuadtreeNode *parent)
{
	int totalLevel = int(log2(this->cellnum) / log2(4));
	int totalPoints = 0;
	for (int i = id*pow(4, (totalLevel - level)); i <= (id + 1) * pow(4, (totalLevel - level)) - 1; i++) {
		totalPoints += this->cellPtr[i].totalPointNum;
	}
	pNode->numPoints = totalPoints;
	pNode->NodeID = id;
	pNode->parent = parent;
	pNode->level = level;
	if ((totalPoints < MAXPOINTINNODE)||(level==totalLevel)) {
		pNode->isLeaf = true;
		pNode->DL = NULL;
		pNode->DR = NULL;
		pNode->UL = NULL;
		pNode->UR = NULL;
		return 0;
	}
	else {
		pNode->isLeaf = false;
		pNode->UL = (QuadtreeNode*)malloc(sizeof(QuadtreeNode));
		this->buildQuadTree(level + 1, id << 2, pNode->UL, pNode);
		pNode->UR = (QuadtreeNode*)malloc(sizeof(QuadtreeNode));
		this->buildQuadTree(level + 1, (id << 2)+1, pNode->UR, pNode);
		pNode->DL = (QuadtreeNode*)malloc(sizeof(QuadtreeNode));
		this->buildQuadTree(level + 1, (id << 2)+2, pNode->DL, pNode);
		pNode->DR = (QuadtreeNode*)malloc(sizeof(QuadtreeNode));
		this->buildQuadTree(level + 1, (id << 2)+3, pNode->DR, pNode);
		return 0;
	}

}

Grid::Grid(const MBB& mbb,float val_cell_size)
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
	for (int i = 0; i <= cellNum_axis - 1; i++) {
		for (int j = 0; j <= cellNum_axis - 1; j++) {
			int cell_idx = getIdxFromXY(j, i);
			cellPtr[cell_idx].initial(i, j, MBB(range.xmin + cell_size*j, range.ymax - cell_size*(i+1), range.xmin + cell_size*(j + 1), range.ymax - cell_size*(i)));
		}
	}
}

//�ѹ켣t������ӹ켣����ӵ�cell����
//��һ�������ǰ��ӹ켣�Ž���cell���棬���һ����item
int Grid::addTrajectoryIntoCell(Trajectory &t)
{
	if (t.length == 0)
		return 1;//�չ켣
	SamplePoint p = t.points[0];
	int lastCellNo = WhichCellPointIn(p); 
	int lastCellStartIdx = 0;
	int nowCellNo;
	//cell based traj���ɣ��ǵ�ת����free��
	vector<int> *tempCellBasedTraj = new vector<int>;
	tempCellBasedTraj->reserve(1048577);
	int tempCellNum = 0;
	for (int i = 1; i <= t.length - 1; i++) {
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
			}
			//������һ�������cell��Ҫ���
			else
			{
				tempCellNum += 2;
				tempCellBasedTraj->push_back(lastCellNo);
				tempCellBasedTraj->push_back(nowCellNo);
				cellPtr[lastCellNo].addSubTra(t.tid, lastCellStartIdx, i - 1, i - 1 - lastCellStartIdx + 1);
				cellPtr[nowCellNo].addSubTra(t.tid, i, i, 1);
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
				cellPtr[lastCellNo].addSubTra(t.tid, lastCellStartIdx, i - 1, i - 1 - lastCellStartIdx + 1);
				lastCellNo = nowCellNo;
				lastCellStartIdx = i;
			}
		}
	}
	this->cellBasedTrajectory[t.tid].length = tempCellNum;
	this->cellBasedTrajectory[t.tid].cellNo = (int*)malloc(sizeof(int)*tempCellNum);
	if (this->cellBasedTrajectory[t.tid].cellNo == NULL) throw("alloc error");
	for (int i = 0; i <= tempCellNum - 1; i++) {
		this->cellBasedTrajectory[t.tid].cellNo[i] = tempCellBasedTraj->at(i);
	}
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

int Grid::addDatasetToGrid(Trajectory * db, int traNum)
{
	//ע�⣬�켣��Ŵ�1��ʼ
	this->cellBasedTrajectory.resize(traNum + 1); //����cellbasedtraj�Ĺ�ģ���ӹ켣��ʱ�����ֱ����
	int pointCount = 0;
	for (int i = 1; i <= traNum; i++) {
		addTrajectoryIntoCell(db[i]);
	}
	for (int i = 0; i <= cellnum - 1; i++) {
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
	this->allPoints = (Point*)malloc(sizeof(Point)*(this->totalPointNum));
	pointCount = 0;
	for (int i = 0; i <= cellnum - 1; i++) {
		cellPtr[i].pointRangeStart = pointCount;
		for (int j = 0; j <= cellPtr[i].subTraNum - 1; j++) {
			for (int k = cellPtr[i].subTraTable[j].startpID; k <= cellPtr[i].subTraTable[j].endpID; k++) {
				allPoints[pointCount].tID = cellPtr[i].subTraTable[j].traID;
				allPoints[pointCount].x = tradb[allPoints[pointCount].tID].points[k].lon;
				allPoints[pointCount].y = tradb[allPoints[pointCount].tID].points[k].lat;
				allPoints[pointCount].time = tradb[allPoints[pointCount].tID].points[k].time;
				pointCount++;
			}
		}
		cellPtr[i].pointRangeEnd = pointCount - 1;
		if (cellPtr[i].pointRangeEnd - cellPtr[i].pointRangeStart + 1 != cellPtr[i].totalPointNum)
			cerr << "Grid.cpp: something wrong in total point statistic" << endl;
	}
	//�����ɺõ�allpoints�ŵ�GPU��
	putCellDataSetIntoGPU(this->allPoints, this->allPointsPtrGPU, this->totalPointNum);


	return 0;
}

int Grid::writeCellsToFile(int * cellNo,int cellNum, string file)
// under editing....
{
	fout.open(file, ios_base::out);
	for (int i = 0; i <= cellNum - 1; i++) {
		int outCellIdx = cellNo[i];
		cout << outCellIdx << ": " << "[" << cellPtr[outCellIdx].mbb.xmin << "," <<cellPtr[outCellIdx].mbb.xmax << "," << cellPtr[outCellIdx].mbb.ymin << "," << cellPtr[outCellIdx].mbb.ymax << "]" << endl;
		for (int j = 0; j <= cellPtr[outCellIdx].subTraNum - 1; j++) {
			int tid = cellPtr[outCellIdx].subTraTable[j].traID;
			int startpid = cellPtr[outCellIdx].subTraTable[j].startpID;
			int endpid = cellPtr[outCellIdx].subTraTable[j].endpID;
			for (int k = startpid; k <= endpid; k++) {
				cout << tradb[tid].points[k].lat << "," << tradb[tid].points[k].lon << ";";
			}
			cout << endl;
		}
	}
	return 0;
}



//int Grid::rangeQuery(MBB & bound, int * ResultTraID, SamplePoint ** ResultTable,int* resultSetSize,int* resultTraLength)
//��Ҫ��д����Ϊ�������ı�
int Grid::rangeQuery(MBB & bound, CPURangeQueryResult * ResultTable, int* resultSetSize)
{
	//�ⲿ��Ҫ��ֲ��gpu�ϣ������õײ㺯��д
	//Ϊ�˿ɱȽϣ�����������ڽ���Ҫ��ѹ켣����������ˣ�result����֯����QueryResult������
	//�ж�range�Ƿ񳬳���ͼ
	ResultTable = (CPURangeQueryResult*)malloc(sizeof(CPURangeQueryResult));
	ResultTable->traid = -1; //table��ͷtraidΪ-1 flag
	ResultTable->next = NULL;
	CPURangeQueryResult* newResult,* nowResult;
	nowResult = ResultTable;
	if (this->range.intersect(bound) != 2)
		return 1;
	else
	{
		int g1, g2, g3, g4; //box�Ķ�����������
		int a, b, c, d;//box�Ķ������ڸ��Ӻ�
		int *candidatesCellID=NULL,*resultsCellID=NULL,*directResultsCellID=NULL;//��ѡ���ӣ�Ĭ��Ϊ��
		int m, n;//mΪgrid������nΪ����
		int candidateSize = 0;//candidate����
		int resultSize,DirectresultSize = 0;//�������
		int counter = 0;//������
		m = this->cellNum_axis;
		n = this->cellNum_axis;
		g1 = (int)((bound.xmin - range.xmin) / cell_size);
		g2 = (int)((bound.xmax - range.xmin) / cell_size);
		g3 = (int)((range.ymax - bound.ymax) / cell_size);
		g4 = (int)((range.ymax - bound.ymin) / cell_size);
		//for test
		//g1 = test[0];
		//g2 = test[1];
		//g3 = test[2];
		//g4 = test[3];
		//m = 10;
		//n = 10;

		a = g1 + g3*m;
		b = g2 + g3*m;
		c = g1 + g4*m;
		d = g2 + g4*m;

		if (a == b){
			candidateSize = (c - a) / m + 1;
		}
		else {
			if (a == c)
				candidateSize = (b - a) + 1;
			else
				candidateSize = ((c - a) / m + 1) * 2 + (b - a + 1) * 2 - 4;
		}
		//��bounding box���߾�����cell����candidates
		candidatesCellID = (int*)malloc(sizeof(int)*candidateSize);
		counter = 0;
		for (int i = a; i <= b; i++) {
			candidatesCellID[counter] = i;
			counter++;
		}
		for (int i = c; i <= d; i++) {
			candidatesCellID[counter] = i;
			counter++;
		}
		if (g4 - g3 >= 2) {
			for (int i = a + m; i <= a + (g4 - g3- 1)*m; i = i + m) {
				candidatesCellID[counter] = i;
				counter++;
			}
			for (int i = b + m; i <= b + (g4 - g3- 1)*m; i = i + m) {
				candidatesCellID[counter] = i;
				counter++;
			}
		}
		if (counter != candidateSize)
			cerr << "size error in range query candidates cell" << endl;

		//һЩֱ����result
		DirectresultSize = (b - a - 1)*(g4 - g3 - 1);
		counter = 0;
		directResultsCellID = (int*)malloc(DirectresultSize * sizeof(int));
		if (b >= a + 2 && c >= a + 2 * m) {
			for (int i = a + 1; i <= b - 1; i++) {
				for (int j = 1; j <= g4 - g3 - 1; j++) {
					directResultsCellID[counter] = i + j*m;
					counter++;
				}
			}
		}
		if (counter != DirectresultSize)
			cerr << "size error in range query directresult cell" <<counter<<","<<candidateSize<< endl;
		timer.start();
		//������candidateCell��⣬�ɲ���
		counter = 0;
		for (int i = 0; i <= candidateSize - 1; i++) {
			Cell &ce = this->cellPtr[candidatesCellID[i]];
			for (int j = 0; j <= ce.subTraNum - 1; j++) {
				int traid = ce.subTraTable[j].traID;
				int startIdx = ce.subTraTable[j].startpID;
				int endIdx = ce.subTraTable[j].endpID;
				for (int k = startIdx; k <= endIdx; k++) {
					if (bound.pInBox(tradb[traid].points[k].lon, tradb[traid].points[k].lat))//�õ���bound��
					{
						newResult = (CPURangeQueryResult*)malloc(sizeof(CPURangeQueryResult));
						if (newResult == NULL)
							return 2; //�����ڴ�ʧ��
						newResult->traid = tradb[traid].points[k].tid;
						newResult->x = tradb[traid].points[k].lon;
						newResult->y = tradb[traid].points[k].lat;
						newResult->next = NULL;
						nowResult->next = newResult;
						nowResult = newResult;
						counter++;
					}
				}
			}
		}
		timer.stop();
		cout << "CPU time:" << timer.elapse() << "ms" <<endl;
		//ֱ����Ϊresult��cell�ӽ�resulttable
		for (int i = 0; i <= DirectresultSize - 1; i++) {
			Cell &ce = this->cellPtr[directResultsCellID[i]];
			for (int j = 0; j <= ce.subTraNum - 1; j++) {
				int traid = ce.subTraTable[j].traID;
				int startIdx = ce.subTraTable[j].startpID;
				int endIdx = ce.subTraTable[j].endpID;
				for (int k = startIdx; k <= endIdx; k++) {
					newResult = (CPURangeQueryResult*)malloc(sizeof(CPURangeQueryResult));
					newResult->traid = tradb[traid].points[k].tid;
					newResult->x = tradb[traid].points[k].lon;
					newResult->y = tradb[traid].points[k].lat;
					newResult->next = NULL;
					nowResult->next = newResult;
					nowResult = newResult;
					counter++;
				}
			}
		}
		(*resultSetSize) = counter;
		//������
	}
	return 0;
}

//��Ҫ��д����Ϊ�������ı�
int Grid::rangeQueryGPU(MBB & bound, CPURangeQueryResult * ResultTable, int * resultSetSize)
{
	//�ⲿ��Ҫ��ֲ��gpu�ϣ������õײ㺯��д
	//Ϊ�˿ɱȽϣ�����������ڽ���Ҫ��ѹ켣����������ˣ�result����֯����QueryResult������
	//�ж�range�Ƿ񳬳���ͼ
	ResultTable = (CPURangeQueryResult*)malloc(sizeof(CPURangeQueryResult));
	ResultTable->traid = -1; //table��ͷtraidΪ-1 flag
	ResultTable->next = NULL;
	CPURangeQueryResult* newResult, *nowResult;
	nowResult = ResultTable;
	if (this->range.intersect(bound) != 2)
		return 1;
	else
	{
		int g1, g2, g3, g4; //box�Ķ�����������
		int a, b, c, d;//box�Ķ������ڸ��Ӻ�
		int *candidatesCellID = NULL, *resultsCellID = NULL, *directResultsCellID = NULL;//��ѡ���ӣ�Ĭ��Ϊ��
		int m, n;//mΪgrid������nΪ����
		int candidateSize = 0;//candidate����
		int resultSize, DirectresultSize = 0;//�������
		int counter = 0;//������
		m = this->cellNum_axis;
		n = this->cellNum_axis;
		g1 = (int)((bound.xmin - range.xmin) / cell_size);
		g2 = (int)((bound.xmax - range.xmin) / cell_size);
		g3 = (int)((range.ymax - bound.ymax) / cell_size);
		g4 = (int)((range.ymax - bound.ymin) / cell_size);
		//for test
		//g1 = test[0];
		//g2 = test[1];
		//g3 = test[2];
		//g4 = test[3];
		//m = 10;
		//n = 10;

		a = g1 + g3*m;
		b = g2 + g3*m;
		c = g1 + g4*m;
		d = g2 + g4*m;

		if (a == b) {
			candidateSize = (c - a) / m + 1;
		}
		else {
			if (a == c)
				candidateSize = (b - a) + 1;
			else
				candidateSize = ((c - a) / m + 1) * 2 + (b - a + 1) * 2 - 4;
		}
		//��bounding box���߾�����cell����candidates
		candidatesCellID = (int*)malloc(sizeof(int)*candidateSize);
		counter = 0;
		for (int i = a; i <= b; i++) {
			candidatesCellID[counter] = i;
			counter++;
		}
		for (int i = c; i <= d; i++) {
			candidatesCellID[counter] = i;
			counter++;
		}
		if (g4 - g3 >= 2) {
			for (int i = a + m; i <= a + (g4 - g3 - 1)*m; i = i + m) {
				candidatesCellID[counter] = i;
				counter++;
			}
			for (int i = b + m; i <= b + (g4 - g3 - 1)*m; i = i + m) {
				candidatesCellID[counter] = i;
				counter++;
			}
		}
		if (counter != candidateSize)
			cerr << "size error in range query candidates cell" << endl;

		//һЩֱ����result
		DirectresultSize = (b - a - 1)*(g4 - g3 - 1);
		counter = 0;
		directResultsCellID = (int*)malloc(DirectresultSize * sizeof(int));
		if (b >= a + 2 && c >= a + 2 * m) {
			for (int i = a + 1; i <= b - 1; i++) {
				for (int j = 1; j <= g4 - g3 - 1; j++) {
					directResultsCellID[counter] = i + j*m;
					counter++;
				}
			}
		}
		if (counter != DirectresultSize)
			cerr << "size error in range query directresult cell" << counter << "," << candidateSize << endl;
		timer.start();
		//������candidateCell��� on CUDA
#ifdef _CELL_BASED_STORAGE
		int* candidateCellRangeStarts = (int*)malloc(sizeof(int)*candidateSize);
		int *candidateCellRangeEnds = (int*)malloc(sizeof(int)*candidateSize);
		int notNullCandidateNum = 0;//candidate Cell��ǿյ���Ŀ��Ϊ�˱���kernel���±����
		for (int i = 0; i <= candidateSize - 1; i++) {
			if (this->cellPtr[candidatesCellID[i]].pointRangeEnd < this->cellPtr[candidatesCellID[i]].pointRangeStart)//���if�ж���Ч����˵�����cell �ǿյ�
				continue;
			//û�б�continue�ľ��Ƿǿյ�
			candidateCellRangeStarts[notNullCandidateNum] = this->cellPtr[candidatesCellID[i]].pointRangeStart;
			candidateCellRangeEnds[notNullCandidateNum] = this->cellPtr[candidatesCellID[i]].pointRangeEnd;
			notNullCandidateNum++;
		}
		Point *resultsCandidate = NULL;
		int resultNum = 0;
		cudaRangeQueryHandler(candidatesCellID, candidateCellRangeStarts, candidateCellRangeEnds, notNullCandidateNum, bound.xmin, bound.ymin, bound.xmax, bound.ymax, resultsCandidate, resultNum,this->allPointsPtrGPU , resultsCandidate);
		//Sleep(1000);
		timer.stop();
		
		cout << "GPU Time:" << timer.elapse() << "ms" << endl;
		//for (int i = 0; i <= resultNum - 1; i++) {
		//	cout << resultIdxs[i] << ",";
		//}
		cout << endl;
#else



		//���Ƚ�candidateCell������չ���Ž��ڴ����ÿһ��point���
		int pointNum = 0;
		for (int i = 0; i <= candidateSize - 1; i++) {
			int subTraNum = this->cellPtr[candidatesCellID[i]].subTraNum;
			for (int j = 0; i <= subTraNum - 1; j++) {
				pointNum += this->cellPtr[candidatesCellID[i]].subTraTable[j].numOfPoint;
			}
		}
		Point* candidatePoints = (Point*)malloc(sizeof(Point)*pointNum);
		counter = 0;
		for (int i = 0; i <= candidateSize - 1; i++) {
			int subTraNum = this->cellPtr[candidatesCellID[i]].subTraNum;
			for (int j = 0; i <= subTraNum - 1; j++) {
				for (int k = this->cellPtr[candidatesCellID[i]].subTraTable[j].startpID; k <= this->cellPtr[candidatesCellID[i]].subTraTable[j].endpID; k++) {
					candidatePoints[counter].tID = this->cellPtr[candidatesCellID[i]].subTraTable[j].traID;
					candidatePoints[counter].x = tradb[candidatePoints[counter].tID].points[k].lon;
					candidatePoints[counter].y = tradb[candidatePoints[counter].tID].points[k].lat;
					candidatePoints[counter].time = tradb[candidatePoints[counter].tID].points[k].time;
					counter++;
				}
			}
		}
		if (counter != pointNum)
			cerr << "pointnum of candidate error" << endl;
		//����CUDA������point��ַ��point��������ѯ��mbb�������ַ���������
		Point* resultPoints = NULL;
		resultSize = 0;
		cudaRangeQueryHandler(candidatePoints, pointNum, bound.xmin, bound.ymin, bound.xmax, bound.ymax, resultPoints, resultSize);
		//CUDA����������Ѿ������ڽ����ַ�����������

#endif // _CELL_BASED_STORAGE
	}
	return 0;
}


int Grid::SimilarityQuery(Trajectory & qTra, Trajectory **candTra, const int candSize, float * EDRdistance)
{
	cout << candSize << endl;
	SPoint *queryTra = (SPoint*)malloc(sizeof(SPoint)*(qTra.length));
	for (int i = 0; i <= qTra.length - 1; i++) {
		queryTra[i].x = qTra.points[i].lon;
		queryTra[i].y = qTra.points[i].lat;
		queryTra[i].tID = qTra.points[i].tid;
	}

	SPoint **candidateTra = (SPoint**)malloc(sizeof(SPoint*)*candSize);

	for (int i = 0; i <= candSize - 1; i++) {
		candidateTra[i] = (SPoint*)malloc(sizeof(SPoint)*(candTra[i]->length)); //���Ե�ʱ����һ�����ܱ��ڴ����FFFFF
		for (int j = 0; j <= candTra[i]->length - 1; j++) {
			candidateTra[i][j].x = candTra[i]->points[j].lon;
			candidateTra[i][j].y = candTra[i]->points[j].lat;
			candidateTra[i][j].tID = candTra[i]->points[j].tid;
		}
	}

	int queryLength=qTra.length;
	int *candidateLength = (int*)malloc(sizeof(int)*candSize);
	for (int i = 0; i <= candSize - 1; i++) {
		candidateLength[i] = candTra[i]->length;
	}

	int* result = (int*)malloc(sizeof(int)*candSize);

	MyTimer timer1;
	timer1.start();

	//CPU
	int *resultCPU = (int*)malloc(sizeof(int)*candSize);
	for (int i = 0; i <= candSize - 1; i++) {
		//ÿ��DP����
		SPoint *CPUqueryTra = queryTra,*CPUCandTra = candidateTra[i];
		int CPUqueryLength = qTra.length, CPUCandLength = candidateLength[i];
		int longest=0;

		const SPoint *tra1, *tra2;
		int len1, len2;
		if (CPUCandLength >= CPUqueryLength) {
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

		if (CPUqueryLength >= longest) {
			longest = CPUqueryLength;
		}
		else
		{
			longest = CPUCandLength;
		}


		int **stateTable = (int**)malloc(sizeof(int*)*(len1 + 1));
		for (int j = 0; j <= len1; j++) {
			stateTable[j] = (int*)malloc(sizeof(int)*(len2 + 1));
		}
		stateTable[0][0] = 0;
		for (int row = 1; row <= len1; row++) {
			stateTable[row][0] = row;
		}
		for (int col = 1; col <= len2; col++) {
			stateTable[0][col] = col;
		}

		for (int row = 1; row <= len1; row++) {
			for (int col = 1; col <= len2; col++) {
				SPoint p1 = tra1[row-1];
				SPoint p2 = tra2[col-1]; //�������ڴ��Ǿۼ����ʵ���
				bool subcost;
				if (((p1.x - p2.x)*(p1.x - p2.x) + (p1.y - p2.y)*(p1.y - p2.y)) < EPSILON) {
					subcost = 0;
				}
				else
					subcost = 1;
				int myState = 0;
				int state_ismatch = stateTable[row-1][col-1] + subcost;
				int state_up = stateTable[row-1][col] + 1;
				int state_left = stateTable[row][col-1] + 1;
				if (state_ismatch < state_up)
					myState = state_ismatch;
				else if (state_left < state_up)
					myState = state_left;
				else
					myState = state_ismatch;

				stateTable[row][col] = myState;
			//	if (row == len1&&col == len2)
					//cout << myState << endl;
			}
		}
		
		resultCPU[i] = stateTable[len1][len2];
		//cout << resultCPU[i] << endl;
	}
	timer1.stop();
	cout << "CPU Similarity Time:" << timer1.elapse() << "ms" << endl;
	//GPU

	timer1.start();
	handleEDRdistance(queryTra, candidateTra, candSize, queryLength, candidateLength, result);
	timer1.stop();
	cout << "GPU Similarity Time:" << timer1.elapse() << "ms" << endl;

	for (int i = 0; i <= candSize - 1; i++) {
		EDRdistance[i] = result[i];
	}
	free(queryTra);
	for (int i = 0; i <= candSize - 1; i++) {
		free(candidateTra[i]);
	}
	free(candidateTra);
	free(candidateLength);
	free(result);

	return 0;
}



Grid::~Grid()
{
}
