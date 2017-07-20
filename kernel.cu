
#include "cuda_runtime.h"
#include "device_launch_parameters.h"

#include "cudaKernel.h"
#include "thrust/device_ptr.h"
#include "thrust/remove.h"
#include <stdio.h>
#include <assert.h>
#include <vector>
#include <iostream>
#include "ConstDefine.h"





inline void __getLastCudaError(const char *errorMessage, const char *file, const int line)
{
	cudaError_t err = cudaGetLastError();

	if (cudaSuccess != err)
	{
		fprintf(stderr, "%s(%i) : getLastCudaError() CUDA error : %s : (%d) %s.\n",
			file, line, errorMessage, (int)err, cudaGetErrorString(err));
			exit(EXIT_FAILURE);
	}
}


//using namespace thrust;
//static const int MAXTHREAD = 512; //ÿ��block�߳���

cudaError_t addWithCuda(int *c, const int *a, const int *b, unsigned int size);
void CUDAwarmUp() {
	CUDA_CALL(cudaSetDeviceFlags(cudaDeviceMapHost));
	CUDA_CALL(cudaSetDevice(0));
	
}


#ifdef _CELL_BASED_STORAGE
int putCellDataSetIntoGPU(Point* pointsPtr, Point*& pointsPtrGPU, int pointNum) {
	
	CUDA_CALL(cudaMalloc((void**)&pointsPtrGPU, pointNum * sizeof(Point))); //�������ݵ��ڴ�
	//debug
	//std::cout << pointNum << std::endl;
	//debug
	CUDA_CALL(cudaMemcpy(pointsPtrGPU, pointsPtr, pointNum * sizeof(Point), cudaMemcpyHostToDevice));//���ݿ�����gpu��
	return 0;
}
__global__ void cudaRangeQuery(int* rangeStarts, int* rangeEnds, int candidateCellNum, const Point* pointsPtr, const float xmin, const float ymin, const float xmax, const float ymax, const int *resultOffset, Point* resultPtrCuda) {
	int cellNo = blockIdx.x; //candidate����ڼ���cell 0,1,2,....
	if (cellNo >= candidateCellNum) return;
	int tid = threadIdx.x;
	if (tid >= 256) return;
	int pointNum = rangeEnds[cellNo] - rangeStarts[cellNo] + 1;//blockҪ��������cell����ô�����
	const int offset = rangeStarts[cellNo];
	for (int i = tid; i <= pointNum - 1; i += MAXTHREAD) {
		float x = pointsPtr[offset + i].x;
		float y = pointsPtr[offset + i].y;
		uint32_t tid = pointsPtr[offset + i].tID;
		uint32_t time = pointsPtr[offset + i].time;
		if (x <= xmax &&x >= xmin&&y <= ymax&&y >= ymin) {
			resultPtrCuda[resultOffset[cellNo] + i].x = x;
			resultPtrCuda[resultOffset[cellNo] + i].y = y;
			resultPtrCuda[resultOffset[cellNo] + i].tID = tid;
			resultPtrCuda[resultOffset[cellNo] + i].time = time;
		}
		else
			resultPtrCuda[resultOffset[cellNo] + i].tID = -1;
	}
}

__global__ void cudaRangeQueryTest(RangeQueryStateTable* stateTable, int stateTableLength, uint8_t* result, 
	const int maxTrajNum) {
	int bID = blockIdx.x;
	int tID = threadIdx.x;
	__shared__ RangeQueryStateTable sharedStateTable;
	// __shared__ uint8_t resultTemp[10000]; //10K
	sharedStateTable = (stateTable[bID]);
	int jobID = sharedStateTable.queryID;
	SPoint *baseAddr = (SPoint*)(sharedStateTable.ptr);
	int candidateNum = sharedStateTable.candidatePointNum;//��block����Ҫ��ѯ�ĵ�ĸ���
	//int resultOffset = bID*maxPointNumInStateTable; //��block�Ľ������ʼ��ַ
	for (int i = 0; i <= candidateNum / MAXTHREAD-1; i++) {
		SPoint p = *(baseAddr + (i*MAXTHREAD + tID));
		//result[i*MAXTHREAD + tID + resultOffset].idx = ((p.x<sharedStateTable.xmax) && (p.x>sharedStateTable.xmin) &&
			//(p.y<sharedStateTable.ymax) && (p.y>sharedStateTable.ymin))*(i*MAXTHREAD + tID);//�����֤ͨ�������ֵΪ�����ţ�����Ϊ0
		//result[i*MAXTHREAD + tID + resultOffset].jobID = bID;
		//result[resultOffset + (i*MAXTHREAD + tID)] = ((p.x<sharedStateTable.xmax) && (p.x>sharedStateTable.xmin) && 
		//		(p.y<sharedStateTable.ymax) && (p.y>sharedStateTable.ymin));
		if((p.x<sharedStateTable.xmax) && (p.x>sharedStateTable.xmin) && (p.y<sharedStateTable.ymax) && (p.y>sharedStateTable.ymin))
			result[jobID*maxTrajNum + p.tID] = 1;
		//�����֤ͨ��������Ӧλ����Ϊ1

		//__syncthreads();
	}
	if (tID < candidateNum - candidateNum / MAXTHREAD * MAXTHREAD) {
		SPoint p = *(baseAddr + (candidateNum / MAXTHREAD * MAXTHREAD + tID));
		//result[candidateNum / MAXTHREAD * MAXTHREAD + tID + resultOffset].idx = ((p.x<sharedStateTable.xmax) && (p.x>sharedStateTable.xmin) &&
		//	(p.y<sharedStateTable.ymax) && (p.y>sharedStateTable.ymin))*(candidateNum / MAXTHREAD * MAXTHREAD + tID);//�����֤ͨ�������ֵΪ�����ţ�����Ϊ0
		//result[candidateNum / MAXTHREAD * MAXTHREAD + tID + resultOffset].jobID = bID;
		//result[resultOffset + (candidateNum / MAXTHREAD * MAXTHREAD + tID)] = ((p.x<sharedStateTable.xmax) &&
		//	(p.x>sharedStateTable.xmin) && (p.y<sharedStateTable.ymax) && (p.y>sharedStateTable.ymin));
		if ((p.x<sharedStateTable.xmax) && (p.x>sharedStateTable.xmin) && (p.y<sharedStateTable.ymax) && (p.y>sharedStateTable.ymin))
			result[jobID*maxTrajNum + p.tID] = 1;
	}
	//else {
	//	//result[candidateNum / MAXTHREAD * MAXTHREAD + tID + resultOffset].idx = 0; //������Ĳ��֣�ֱ����Ϊ��Ч����
	//}
	//__syncthreads();
	//__syncthreads();
	//int globalTID = blockDim.x * blockIdx.x + threadIdx.x;
	//if (globalTID < stateTableLength) {

	//}
}

int cudaRangeQueryTestHandler(RangeQueryStateTable* stateTableGPU, int stateTableLength, uint8_t *result, int maxTrajNum
	, int maxJobNum, cudaStream_t stream) {
	//RangeQueryResultGPU* resultGPU;
	//MyTimer timer;
	uint8_t* resultGPU;
	//int resultByteNum = (maxPointNum)/8+1;//ÿ�������Ҫ�ü���byte���棬���ܰ����أ�ֻ�ܰ��ֽ�
	CUDA_CALL(cudaMalloc((void**)&resultGPU, (maxTrajNum)*maxJobNum));//selective��һ��
	CUDA_CALL(cudaMemset(resultGPU, 0, (maxTrajNum)*maxJobNum));
	//timer.start();
	//�����һ���ڴ棬ÿ��stateTable��ռ�ݵ��ڴ������
	//CUDA_CALL(cudaMalloc((void**)&resultGPU, (maxPointNum)*stateTableLength));
	
	//CUDA_CALL(cudaMalloc((void**)&resultGPU, maxPointNum*stateTableLength*sizeof(RangeQueryResultGPU)));
	//timer.stop();
	//std::cout << "Time 1:" << timer.elapse() << "ms" << std::endl;

	//timer.start();	
	cudaRangeQueryTest <<<stateTableLength, MAXTHREAD,0, stream >>>(stateTableGPU, stateTableLength, resultGPU, maxTrajNum);
	CUDA_CALL(cudaDeviceSynchronize());
	//timer.stop();
	//std::cout << "Time 2:" << timer.elapse() << "ms" << std::endl;

	//timer.start();
	
	CUDA_CALL(cudaMemcpy(result, resultGPU, (maxTrajNum)*maxJobNum, cudaMemcpyDeviceToHost));

	//timer.stop();
	//std::cout << "Time 3:" << timer.elapse() << "ms" << std::endl;
	return 0;
}

int cudaRangeQueryHandler(int* candidateCells, int* rangeStarts, int* rangeEnds, int candidateCellNum,float xmin, float ymin, float xmax, float ymax, Point*& resultsGPU, int& resultNum,Point *pointsPtrGPU,Point *&result) {
	//��һ��������ʱû���ã�ע������candidatecells[i]�Ѿ�������cell��id������ֻ��ǿ�
	//���ĸ�������ʾ�ǿյ�cell����
	//����candidate���еĵ��������gpu�ڿ�����ͬ��С�Ŀռ���flag��rangestart��rangeend����Ӧcandidatecell�ڵĲ�������AllPoints����ʼ�±����ֹ�±�
	//�����������͵����ڶ��������������һ���Ǳ�������GPU��ַ���ڶ����ǽ���ĸ���
	//PointsPtrGPU�����ݼ���gpu�ĵ�ַ
	//MyTimer timer1;
	//timer1.start();
	int counter = 0;
	int *resultOffset = (int*)malloc(sizeof(int)*candidateCellNum);
	//std::cout << candidateCellNum << ":"<<std::endl;
	for (int i = 0; i <= candidateCellNum - 1; i++) {
		resultOffset[i] = counter;
		////debug
		//std::cout << "(" << rangeStarts[i] << "," << rangeEnds[i] << ");"<<"["<<resultOffset[i]<<"]";
		////debug
		counter += rangeEnds[i] - rangeStarts[i] + 1;
	}
	int totalPointNumInCandidate = counter;


	int *rangeStartsCuda = NULL, *rangeEndsCuda = NULL, *resultOffsetCuda = NULL;

	CUDA_CALL(cudaMalloc((void**)&resultsGPU, sizeof(Point)*totalPointNumInCandidate));
	//��range��cell��Ϣд��gpu
	//CUDA_CALL(cudaMalloc((void**)&candidateCellsCuda, sizeof(int)*candidateCellNum));
	CUDA_CALL(cudaMalloc((void**)&rangeStartsCuda, candidateCellNum*sizeof(int)));
	//std::cout << "\n" << candidateCellNum*sizeof(int) << "\n";
	CUDA_CALL(cudaMalloc((void**)&rangeEndsCuda, candidateCellNum*sizeof(int)));
	CUDA_CALL(cudaMalloc((void**)&resultOffsetCuda, candidateCellNum*sizeof(int)));
	//CUDA_CALL(cudaMemcpy(candidateCellsCuda, candidateCells, candidateCellNum*sizeof(int), cudaMemcpyHostToDevice));
	CUDA_CALL(cudaMemcpy(rangeStartsCuda, rangeStarts, candidateCellNum*sizeof(int), cudaMemcpyHostToDevice));

	CUDA_CALL(cudaMemcpy(rangeEndsCuda, rangeEnds, candidateCellNum*sizeof(int), cudaMemcpyHostToDevice));
	CUDA_CALL(cudaMemcpy(resultOffsetCuda, resultOffset, candidateCellNum*sizeof(int), cudaMemcpyHostToDevice));
	////debug
	//CUDA_CALL(cudaMemcpy( rangeStarts, rangeStartsCuda, candidateCellNum*sizeof(int), cudaMemcpyDeviceToHost));
	//CUDA_CALL(cudaMemcpy( rangeEnds, rangeEndsCuda, candidateCellNum*sizeof(int), cudaMemcpyDeviceToHost));
	//CUDA_CALL(cudaMemcpy( resultOffset, resultOffsetCuda, candidateCellNum*sizeof(int), cudaMemcpyDeviceToHost));
	//for (int i = 0; i <= candidateCellNum - 1; i++) {
	//	//debug
	//	std::cout << "(" << rangeStarts[i] << "," << rangeEnds[i] << ");" << "[" << resultOffset[i] << "]";
	//	//debug
	//}
	//debug
	//timer1.stop();
	//std::cout << timer1.ticks() << std::endl;
	//timer1.start();
	//����kernel�����ĳ����������������Ӧλ��д����AllPoints�е��±꣬����д��-1
	//ÿ��cell�����һ��block
	cudaRangeQuery <<<candidateCellNum, MAXTHREAD >>>(rangeStartsCuda, rangeEndsCuda, candidateCellNum, pointsPtrGPU, xmin, ymin, xmax, ymax, resultOffsetCuda, resultsGPU);
	//kernel���ý��������������idxsGPU�У����������������ӦԪ����������AllPoints���±꣬��������ϣ�����Ϊ-1
	//CUDA_CALL(cudaFree(candidateCellsCuda));
	CUDA_CALL(cudaFree(rangeStartsCuda));
	CUDA_CALL(cudaFree(rangeEndsCuda));
	CUDA_CALL(cudaFree(resultOffsetCuda));
	//getLastCudaError("Error in Calling 'kernel'");
	//ʹ��Thrustɾ������-1���õ����ս��
	//timer1.stop();
	//std::cout << timer1.ticks() << std::endl;

	//���н���ϲ�
	//test

	//timer1.start();
	Point *resultset = NULL;
	resultset = (Point*)malloc(totalPointNumInCandidate*sizeof(Point));
	CUDA_CALL(cudaMemcpy(resultset, resultsGPU, sizeof(Point)*totalPointNumInCandidate, cudaMemcpyDeviceToHost));
	std::vector<Point> *resultPoint = new std::vector<Point>;
	for (int i = 0; i <= totalPointNumInCandidate - 1; i++) {
		if (resultset[i].tID != -1)
		{
			resultPoint->push_back(resultset[i]);
		}
	}
	result = &resultPoint->at(0);
	free(resultset);
	//test
	//timer1.stop();
	//std::cout << timer1.ticks() << std::endl;
	
	//���н���ϲ�
	
	//thrust::device_ptr<int> idxsPtr = thrust::device_pointer_cast(idxsGPU);
	//int a;
	//cudaMemcpy(&a, idxsGPU, 1, cudaMemcpyDeviceToHost);
	//size_t num = thrust::remove(idxsPtr, idxsPtr + totalPointNumInCandidate-1, -1) - idxsPtr;
	//int *result = (int*)malloc(sizeof(int)*num);
	//thrust::copy(idxsPtr, idxsPtr + num, result);
	//resultNum = num;
	//resultIdx = result;

	//CUDA_CALL(cudaFree(idxsGPU));


	return 0;
}





#else
int cudaRangeQueryHandler(Point* pointsPtr, int pointNum, float xmin, float ymin, float xmax, float ymax, Point*& resultsPtr, int& resultNum) {
	Point* pointsPtrCuda = NULL;
	Point* resultPtrCuda = NULL;
	CUDA_CALL(cudaMalloc((void**)&pointsPtrCuda, pointNum * sizeof(Point))); //�������ݵ��ڴ�
	CUDA_CALL(cudaMalloc((void**)&resultPtrCuda, pointNum * sizeof(Point))); //gpu�ڴ洢����ĵط�
	CUDA_CALL(cudaMemcpy(pointsPtrCuda, pointsPtr, pointNum * sizeof(Point), cudaMemcpyHostToDevice));//���ݿ�����gpu��

																									  //���ú˺����������ݣ��������gpu��

																									  //ȡ�����ݣ�����
	return 0;
}
#endif


__global__ void addKernel(int *c, const int *a, const int *b)
{
    int i = threadIdx.x;
    c[i] = a[i] + b[i];
}

//int main()
//{
//    const int arraySize = 5;
//    const int a[arraySize] = { 1, 2, 3, 4, 5 };
//    const int b[arraySize] = { 10, 20, 30, 40, 50 };
//    int c[arraySize] = { 0 };
//
//    // Add vectors in parallel.
//    cudaError_t cudaStatus = addWithCuda(c, a, b, arraySize);
//    if (cudaStatus != cudaSuccess) {
//        fprintf(stderr, "addWithCuda failed!");
//        return 1;
//    }
//
//    printf("{1,2,3,4,5} + {10,20,30,40,50} = {%d,%d,%d,%d,%d}\n",
//        c[0], c[1], c[2], c[3], c[4]);
//
//    // cudaDeviceReset must be called before exiting in order for profiling and
//    // tracing tools such as Nsight and Visual Profiler to show complete traces.
//    cudaStatus = cudaDeviceReset();
//    if (cudaStatus != cudaSuccess) {
//        fprintf(stderr, "cudaDeviceReset failed!");
//        return 1;
//    }
//
//    return 0;
//}

// Helper function for using CUDA to add vectors in parallel.