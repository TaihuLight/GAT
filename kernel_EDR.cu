//����EDR��GPU�����㷨
//zbw0046 3.22



#include "cuda_runtime.h"
#include "device_launch_parameters.h"
#include <iostream>
#include <stdio.h>
#include "ConstDefine.h"
#include "cudaKernel.h"
#include <assert.h>
#include <stdlib.h>
#include"device_functions.h"
#include "WinTimer.h"

#define CUDA_CALL(x) { const cudaError_t a = (x); if (a!= cudaSuccess) { printf("\nCUDA Error: %s(err_num=%d)\n", cudaGetErrorString(a), a); cudaDeviceReset(); assert(0);}}


/*
���м���1�����ģdp
��Ҫ��ǰ����ǰ����dp�Ľ���������ڹ����ڴ���
iter: �ڼ���dp��λ��outputIdx����������ȫ���ڴ�λ�ã�tra1��tra2�������켣����ǰ�����빲���ڴ棻
*/
//__global__ void DPforward(const int iter, const int* outputIdx,const SPoint *tra1,const SPoint *tra2) {
//	SPoint p1 = tra1[threadIdx.x];
//	SPoint p2 = tra2[iter - threadIdx.x - 1]; //�������ڴ��Ǿۼ����ʵ���
//	bool subcost;
//	if (((p1.x - p2.x)*(p1.x - p2.x) + (p1.y - p2.y)*(p1.y - p2.y)) < EPSILON) {
//		subcost = 0;
//	}
//	else
//		subcost = 1;
//
//}

/*
SPoint�汾
case1���켣����С��512
���м���n��DP
��Ҫ��ǰ����ǰ����dp�Ľ���������ڹ����ڴ���
queryTra[],candidateTra[][]:�켣
stateTableGPU[][]:��ÿ��candidate��state��
result[]:����ÿ��candidate��EDR���
�Ż�����
1���켣����share memory����
2��ֱ�Ӵ��ݹ켣����ʹ��ָ��
*/
__global__ void EDRDistance_1(SPoint *queryTra, SPoint **candidateTra,int candidateNum,int queryLength,int *candidateLength,int** stateTableGPU,int *result) {
	int blockID = blockIdx.x;
	int threadID = threadIdx.x;
	if (blockID >= candidateNum) return;
	if ((threadID >= candidateLength[blockID]) && (threadID >= queryLength)) return;
	const int lenT = candidateLength[blockID];
	//int iterNum = queryLength;
	//if (lenT > queryLength)
	//	iterNum = lenT;
	const int iterNum = queryLength + lenT - 1;
	__shared__ int state[2][MAXTHREAD]; //���ڴ洢ǰ���εĽ��
	state[0][0] = 0;
	state[1][0] = 1;
	state[1][1] = 1;
	//�������켣���򣬱�֤��һ���ȵڶ�����
	//���Ȱѹ켣���ڹ����ڴ���
	__shared__ SPoint queryTraS[MAXTHREAD];
	__shared__ SPoint traData[MAXTHREAD];
	if (threadID < lenT) {
		traData[threadID] = candidateTra[blockID][threadID];
	}
	if (threadID < queryLength) {
		queryTraS[threadID] = queryTra[threadID];
	}
	const SPoint *tra1, *tra2; //��֤tra1��tra2��
	int len1, len2;
	if (lenT >= queryLength) {
		tra1 = queryTraS;
		tra2 = traData;
		len1 = queryLength;
		len2 = lenT;
	}
	else
	{
		tra1 = traData;
		tra2 = queryTraS;
		len1 = lenT;
		len2 = queryLength;
	}

	int myState;
	for (int i = 0; i <= iterNum - 1; i++) {//��i��dp
		if (i < len1 - 1) {
			if (threadID <= i) {
				SPoint p1 = tra1[threadID];
				SPoint p2 = tra2[i - threadID]; //�������ڴ��Ǿۼ����ʵ���
				bool subcost;
				//if (((p1.x - p2.x)*(p1.x - p2.x) + (p1.y - p2.y)*(p1.y - p2.y)) < EPSILON) {
				//	subcost = 0;
				//}
				//else
				//	subcost = 1;
				subcost = !(((p1.x - p2.x)*(p1.x - p2.x) + (p1.y - p2.y)*(p1.y - p2.y)) < EPSILON);
				int state_ismatch = state[0][threadID] + subcost;
				int state_up = state[1][threadID] + 1;
				int state_left = state[1][threadID+1] + 1;
				if (state_ismatch < state_up)
					myState = state_ismatch;
				else if (state_left < state_up)
					myState = state_left;
				else
					myState = state_up;
				//ȥ��if�ı�﷽ʽ���Ƿ�����������ܣ�
				//myState = (state_ismatch < state_up) * state_ismatch + (state_left < state_up) * state_up + (state_left >= state_up) * state_left;
				
			}
		}
		else if (i > iterNum - len1) {
			if (threadID <= iterNum - i - 1) {
				SPoint p1 = tra1[threadID+len1-(iterNum-i)];
				SPoint p2 = tra2[len2-1-threadID]; //�������ڴ��Ǿۼ����ʵ���
				bool subcost;
				if (((p1.x - p2.x)*(p1.x - p2.x) + (p1.y - p2.y)*(p1.y - p2.y)) < EPSILON) {
					subcost = 0;
				}
				else
					subcost = 1;
				int state_ismatch = state[0][threadID+1] + subcost;
				int state_up = state[1][threadID] + 1;
				int state_left = state[1][threadID + 1] + 1;
				if (state_ismatch < state_up)
					myState = state_ismatch;
				else if (state_left < state_up)
					myState = state_left;
				else
					myState = state_up;
			}
		}
		else
		{
			if (threadID < len1) {
				SPoint p1 = tra1[threadID];
				SPoint p2 = tra2[i-threadID]; //�������ڴ��Ǿۼ����ʵ���
				bool subcost;
				if (((p1.x - p2.x)*(p1.x - p2.x) + (p1.y - p2.y)*(p1.y - p2.y)) < EPSILON) {
					subcost = 0;
				}
				else
					subcost = 1;
				int state_ismatch = state[0][threadID] + subcost;
				int state_up = state[1][threadID] + 1;
				int state_left = state[1][threadID + 1] + 1;
				if (state_ismatch < state_up)
					myState = state_ismatch;
				else if (state_left < state_up)
					myState = state_left;
				else
					myState = state_up;
			}
		}
		//дmyState��share�ڴ�,ckecked
		int startidx;
		//���Ƚ�������д��ȫ���ڴ棬ȫд
		//startidx�Ǿɵ�����Ӧ����ȫ���ڴ��е�ַ����i-2����
		//����Ӧд��ȫ���ڴ����ʼλ��

		if (i-2 < len1 - 2) {
			startidx = (i-2 + 2)*(i-2 + 3) / 2;
			if (threadID <= i) {
				stateTableGPU[blockID][threadID + startidx] = state[0][threadID];
			}
		}
		else if (i-2 >= iterNum - len1) {
			startidx = (len1 + 1)*(len2 + 1) - (iterNum - (i-2))*(iterNum - (i-2) + 1) / 2;
			if (threadID <= iterNum - i + 1 ) {
				stateTableGPU[blockID][threadID + startidx] = state[0][threadID];
			}
		}
		else
		{
			startidx = (len1 + 1)*((i - 2) - (len1 - 2)) + len1*(len1 + 1) / 2;
			if (threadID <= len1) {
				stateTableGPU[blockID][threadID + startidx] = state[0][threadID];
			}
		}

		//�ƶ������ݵ�������
		state[0][threadID] = state[1][threadID];
		//д��������
		if (i < len1-1) {
			if (threadID <= i)
				state[1][threadID + 1] = myState;
			if (threadID == 0) {
				state[1][0] = i + 2;
				state[1][i + 2] = i + 2;
			}
		}
		else if (i >= iterNum - len1) {
			if (threadID <= iterNum - i - 1)
				state[1][threadID] = myState;
		}
		else
		{
			if (threadID < len1)
				state[1][threadID + 1] = myState;
			if (threadID == 0) {
				state[1][0] = i + 2;
			}
		}
		__syncthreads();
	}
	//�����������һ�μ���һ�����ɽ���0��ɵ�
	if (threadID == 0)
		result[blockID] = myState;
}


//__global__ void testSharedMemory()
//{
//	__shared__ SPoint queryTraS[MAXLENGTH];
//	__shared__ SPoint traData[MAXLENGTH];
//	__shared__ SPoint traData2[MAXLENGTH];
//	SPoint s;
//	s.x = 4;
//	s.y = 5;
//	traData[1535] = s;
//	queryTraS[1535] = s;
//	traData2[1535] = s;
//}


/*
SPoint�汾
ͬʱ�������ɸ�query��EDR��������һ��EDR����Ϊ��λ��ÿ��block����һ��EDR��thread����һ��б����state�Ĳ��м��㡣
case1���켣���ȿɳ���512������ѭ���������512�ġ�
���м���n��DP
��Ҫ��ǰ����ǰ����dp�Ľ���������ڹ����ڴ���
queryTaskNum:�ܹ��м���EDR��������
queryTaskInfo[]��ÿ��task��Ӧ��qID��candidateID��Ϣ����struct�洢
queryTra[],candidateTra[]:�켣���ݣ�candidateTra��֤���ڲ��켣���ظ�
queryTraOffset[],candidateTraOffset[]:ÿ���켣��offset��candidateTra��֤���ڲ��켣���ظ�
queryLength[],candidateLength[]:ÿ���켣�ĳ��ȣ���ʵoffset������ǳ��ȣ�����idx������Ķ�Ӧ
����candidateLength[id]�ǵ�id��candidate Traj�ĳ���
stateTableGPU[][]:��ÿ��candidate��state��
result[]:����ÿ��candidate��EDR���
�Ż�����
1���켣����share memory����
2��ֱ�Ӵ��ݹ켣����ʹ��ָ��
*/



__global__ void EDRDistance_Batch(int queryTaskNum, TaskInfoTableForSimilarity* taskInfoTable, SPoint *queryTra, int* queryTraOffset, SPoint** candidateTraOffsets, int* queryLength, int *candidateLength, int *result) {
	int blockID = blockIdx.x;
	int threadID = threadIdx.x;
	if (blockID >= queryTaskNum) return;
	int thisQueryID = taskInfoTable[blockID].qID;
	int thisQueryLength = queryLength[thisQueryID];
	if ((threadID >= candidateLength[blockID]) && (threadID >= thisQueryLength)) return;
	const int lenT = candidateLength[blockID];
	//int iterNum = queryLength;
	//if (lenT > queryLength)
	//	iterNum = lenT;
	const int iterNum = thisQueryLength + lenT - 1;
	__shared__ int state[2][MAXLENGTH+1]; //���ڴ洢ǰ���εĽ����ռ��8KB��
	state[0][0] = 0;
	state[1][0] = 1;
	state[1][1] = 1;
	//�������켣���򣬱�֤��һ���ȵڶ�����
	//���Ȱѹ켣���ڹ����ڴ���
	//����������share memory�Ƿ��õ����⣬����д����64KB��Ȼ��K80�ƺ���512KB
	//�����64KB�Ļ���ÿ���켣�1024���㣨�����켣��ռ��24KB��
	//__shared__ SPoint queryTraS[MAXLENGTH];
	//__shared__ SPoint traData[MAXLENGTH];


	//for (int i = 0; i <= lenT - 1;i+=MAXTHREAD)
	//{
	//	if(threadID+i<lenT)
	//	{
	//		traData[threadID + i] = SPoint(candidateTraOffsets[blockID][threadID + i]);
	//	}
	//}

	SPoint* queryTraBaseAddr = queryTra + queryTraOffset[thisQueryID];
	//for (int i = 0; i <= thisQueryLength - 1;i+=MAXTHREAD)
	//{
	//	if(threadID+i<thisQueryLength)
	//	{
	//		queryTraS[threadID + i] = *(queryTraBaseAddr + threadID + i);
	//	}
	//}
	SPoint *queryTraS = queryTraBaseAddr;
	SPoint *traData = candidateTraOffsets[blockID];
	const SPoint *tra1, *tra2; //��֤tra1��tra2��
	int len1, len2;
	if (lenT >= thisQueryLength) {
		tra1 = queryTraS;
		tra2 = traData;
		len1 = thisQueryLength;
		len2 = lenT;
	}
	else
	{
		tra1 = traData;
		tra2 = queryTraS;
		len1 = lenT;
		len2 = thisQueryLength;
	}

	int myState[5];
	int nodeID;
	for (int i = 0; i <= iterNum - 1; i++) {//��i��dp
		if (i < len1 - 1) {
			for (int startIdx = 0; startIdx <= i; startIdx += MAXTHREAD) {
				nodeID = startIdx + threadID;
				if (nodeID <= i) {
					SPoint p1 = tra1[nodeID];
					SPoint p2 = tra2[i - nodeID]; //�������ڴ��Ǿۼ����ʵ���
					bool subcost;
					if (((p1.x - p2.x)*(p1.x - p2.x) + (p1.y - p2.y)*(p1.y - p2.y)) < EPSILON) {
						subcost = 0;
					}
					else
						subcost = 1;
					//subcost = !(((p1.x - p2.x)*(p1.x - p2.x) + (p1.y - p2.y)*(p1.y - p2.y)) < EPSILON);
					int state_ismatch = state[0][nodeID] + subcost;
					int state_up = state[1][nodeID] + 1;
					int state_left = state[1][nodeID + 1] + 1;
					bool c1 = ((state_ismatch < state_up) && (state_ismatch < state_left));
					bool c2 = ((state_left < state_up) && ((state_left < state_ismatch)));
					//ȥ��if�ı�﷽ʽ���Ƿ�����������ܣ�
					myState[nodeID / MAXTHREAD] = c1 * state_ismatch + c2 * state_left + !(c1 || c2) * state_up;
					//if ((state_ismatch < state_up) && (state_ismatch < state_left))
					//	myState[nodeID/MAXTHREAD] = state_ismatch;
					//else if ((state_left < state_up) && ((state_left < state_ismatch)))
					//	myState[nodeID / MAXTHREAD] = state_left;
					//else
					//	myState[nodeID / MAXTHREAD] = state_up;
					////ȥ��if�ı�﷽ʽ���Ƿ�����������ܣ�
					//myState[nodeID / MAXTHREAD] = (state_ismatch < state_up) && (state_ismatch < state_left) * state_ismatch + ((state_left < state_up) && ((state_left < state_ismatch))) * state_left + !(((state_ismatch < state_up) && (state_ismatch < state_left))||(((state_left < state_up) && ((state_left < state_ismatch))))) * state_up;
				}
			}
		}
		else if (i > iterNum - len1) {
			for (int startIdx = 0; startIdx <= iterNum - i - 1; startIdx += MAXTHREAD) {
				nodeID = startIdx + threadID;
				if (nodeID <= iterNum - i - 1) {
					SPoint p1 = tra1[nodeID + len1 - (iterNum - i)];
					SPoint p2 = tra2[len2 - 1 - nodeID]; //�������ڴ��Ǿۼ����ʵ���
					bool subcost;
					if (((p1.x - p2.x)*(p1.x - p2.x) + (p1.y - p2.y)*(p1.y - p2.y)) < EPSILON) {
						subcost = 0;
					}
					else
						subcost = 1;
					int state_ismatch = state[0][nodeID + 1] + subcost;
					int state_up = state[1][nodeID] + 1;
					int state_left = state[1][nodeID + 1] + 1;
					//if (state_ismatch < state_up)
					//	myState[nodeID / MAXTHREAD] = state_ismatch;
					//else if (state_left < state_up)
					//	myState[nodeID / MAXTHREAD] = state_left;
					//else
					//	myState[nodeID / MAXTHREAD] = state_up;
					bool c1 = ((state_ismatch < state_up) && (state_ismatch < state_left));
					bool c2 = ((state_left < state_up) && ((state_left < state_ismatch)));
					//ȥ��if�ı�﷽ʽ���Ƿ�����������ܣ�
					myState[nodeID / MAXTHREAD] = c1 * state_ismatch + c2 * state_left + !(c1 || c2) * state_up;
				}
			}
		}
		else
		{
			for (int startIdx = 0; startIdx <= len1; startIdx += MAXTHREAD) {
				nodeID = startIdx + threadID;
				if (nodeID <= len1) {
					SPoint p1 = tra1[nodeID];
					SPoint p2 = tra2[i - nodeID]; //�������ڴ��Ǿۼ����ʵ���
					bool subcost;
					if (((p1.x - p2.x)*(p1.x - p2.x) + (p1.y - p2.y)*(p1.y - p2.y)) < EPSILON) {
						subcost = 0;
					}
					else
						subcost = 1;
					int state_ismatch = state[0][nodeID] + subcost;
					int state_up = state[1][nodeID] + 1;
					int state_left = state[1][nodeID + 1] + 1;
					//if (state_ismatch < state_up)
					//	myState[nodeID / MAXTHREAD] = state_ismatch;
					//else if (state_left < state_up)
					//	myState[nodeID / MAXTHREAD] = state_left;
					//else
					//	myState[nodeID / MAXTHREAD] = state_up;
					bool c1 = ((state_ismatch < state_up) && (state_ismatch < state_left));
					bool c2 = ((state_left < state_up) && ((state_left < state_ismatch)));
					//ȥ��if�ı�﷽ʽ���Ƿ�����������ܣ�
					myState[nodeID / MAXTHREAD] = c1 * state_ismatch + c2 * state_left + !(c1 || c2) * state_up;
				}
			}
		}
		//дmyState��share�ڴ�,ckecked
		
		//int startidx;
		////���Ƚ�������д��ȫ���ڴ棬ȫд
		////startidx�Ǿɵ�����Ӧ����ȫ���ڴ��е�ַ����i-2����
		////����Ӧд��ȫ���ڴ����ʼλ��
		//// 7.2 ���֣��ƺ�stateTableGPU������д������������
		////////

		//if (i - 2 < len1 - 2) {
		//	startidx = (i - 2 + 2)*(i - 2 + 3) / 2;
		//	for (int Idx = 0; Idx <= i; Idx += MAXTHREAD) {
		//		//if (threadID <= i) {
		//		if(Idx + threadID <= i){
		//			//stateTableGPU[blockID][threadID + startidx] = state[0][threadID];
		//			stateTableGPU[blockID][Idx + threadID + startidx] = state[0][threadID + Idx];
		//		}
		//	}
		//}
		//else if (i - 2 >= iterNum - len1) {
		//	startidx = (len1 + 1)*(len2 + 1) - (iterNum - (i - 2))*(iterNum - (i - 2) + 1) / 2;
		//	for (int Idx = 0; Idx <= iterNum - i + 1; Idx += MAXTHREAD) {
		//		//if (threadID <= iterNum - i + 1) {
		//		if (threadID + Idx <= iterNum - i + 1) {
		//			//stateTableGPU[blockID][threadID + startidx] = state[0][threadID];
		//			stateTableGPU[blockID][Idx + threadID + startidx] = state[0][Idx + threadID];
		//		}
		//	}
		//}
		//else
		//{
		//	startidx = (len1 + 1)*((i - 2) - (len1 - 2)) + len1*(len1 + 1) / 2;
		//	for (int Idx = 0; Idx <= len1; Idx += MAXTHREAD) {
		//		//if (threadID <= len1) {
		//		if (threadID + Idx <= len1) {
		//			// stateTableGPU[blockID][threadID + startidx] = state[0][threadID];
		//			stateTableGPU[blockID][Idx + threadID + startidx] = state[0][Idx + threadID];
		//		}
		//	}
		//}

		//�ƶ������ݵ�������
		for (int Idx = 0; Idx < MAXLENGTH;Idx+=MAXTHREAD)
		{
			state[0][threadID+Idx] = state[1][threadID+Idx];
		}
		//state[0][threadID] = state[1][threadID];

		//д��������
		if (i < len1 - 1) {
			//if (threadID <= i)
			//	state[1][threadID + 1] = myState;
			//if (threadID == 0) {
			//	state[1][0] = i + 2;
			//	state[1][i + 2] = i + 2;
			//}
			for (int Idx = 0; Idx <= i; Idx += MAXTHREAD) {
				if (threadID + Idx <= i)
					state[1][Idx + threadID + 1] = myState[Idx/MAXTHREAD];
			}
			if (threadID == 0) {
				state[1][0] = i + 2;
				state[1][i + 2] = i + 2;
			}
		}
		else if (i >= iterNum - len1) {
			//if (threadID <= iterNum - i - 1)
			//	state[1][threadID] = myState;
			for (int Idx = 0; Idx <= iterNum - i - 1; Idx += MAXTHREAD) {
				if (threadID + Idx <= iterNum - i - 1)
					state[1][threadID + Idx] = myState[Idx / MAXTHREAD];
			}
		}
		else
		{
			//if (threadID < len1)
			//	state[1][threadID + 1] = myState;
			//if (threadID == 0) {
			//	state[1][0] = i + 2;
			//}
			for (int Idx = 0; Idx <= len1; Idx += MAXTHREAD) {
				if (threadID + Idx < len1)
					state[1][Idx + threadID + 1] = myState[Idx / MAXTHREAD];
			}
			if (threadID == 0) {
				state[1][0] = i + 2;
			}
		}
		__syncthreads();
	}
	//�����������һ�μ���һ�����ɽ���0��ɵ�
	if (threadID == 0 && blockID < queryTaskNum)
		result[blockID] = myState[0];
}

int EDRDistance_Batch_Handler(int queryTaskNum, TaskInfoTableForSimilarity* taskInfoTable, SPoint *queryTra, int* queryTraOffset, SPoint** candidateTraOffsets, int* queryLength, int *candidateLength, int *result, cudaStream_t *stream)
{
	EDRDistance_Batch <<<queryTaskNum, MAXTHREAD,0 , *stream >>>(queryTaskNum, taskInfoTable, queryTra, queryTraOffset, candidateTraOffsets, queryLength, candidateLength, result);
	return 0;
}

__device__ inline int binary_search_intPair(intPair* temp, int left,int right,int val)
{
	int mid = (left + right) / 2;
	while(left<=right)
	{
		mid = (left + right) / 2;
		if (temp[mid].int_1 == val)
			return temp[mid].int_2;
		else if (temp[mid].int_1 > val)
		{
			right = mid-1;
		}
		else
			left = mid+1;
	}
	return 0;
}

__device__ inline int binary_search_intPair_Neighbor(intPair* temp, int left, int right, int val)
{
	int mid = (left + right) / 2;
	while (left <= right)
	{
		mid = (left + right) / 2;
		if (temp[mid].int_1 == val)
			return mid;
		else if (temp[mid].int_1 > val)
		{
			right = mid - 1;
		}
		else
			left = mid + 1;
	}
	return -1;
}

// -1Ϊû�ҵ�
__device__ inline int binary_search_int(int* temp, int left, int right, int val)
{
	int mid = (left + right) / 2;
	while (left <= right)
	{
		mid = (left + right) / 2;
		if (temp[mid] == val)
			return mid;
		else if (temp[mid] > val)
		{
			right = mid - 1;
		}
		else
			left = mid + 1;
	}
	return -1;
}

__device__ inline int getIdxFromXYGPU(int x, int y)
{
	int lenx, leny;
	if (x == 0)
		lenx = 1;
	else
	{
		lenx = int(log2f(x)) + 1;
	}
	if (y == 0)
		leny = 1;
	else
		leny = int(log2f(y)) + 1;
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

__device__ inline int findNeighborGPU(int cellNum, int cellID, int * neighborID)
{
	int x = 0, y = 0;
	for (int bit = 0; bit <= int(log2f(cellNum)) - 1; bit++) {
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
				neighborID[cnt++] = getIdxFromXYGPU(xx, yy);
			//printf("%d\t", cnt);
		}
	}
	return 0;
}

__device__ inline bool isPositive(short x)
{
	return x >= 0;
}

__global__ void Calculate_FD_Sparse(intPair* queryFVGPU, intPair* FVinfo, intPair* FVTable, intPair* SubbedArray, intPair* SubbedArrayOffset, int SubbedArrayJump, int queryCellLength, int startTrajIdx, int checkNum, int cellNum, int trajNumInDB, int nonZeroFVNumInDB, short* FDistance)
{
	//��һ�׶Σ����м���
	const int MAX_QUERY_CELLNUMBER = 512;
	int blockID = blockIdx.x;
	int threadID = threadIdx.x;
	int threadIDGlobal = blockDim.x*blockID + threadID;

	__shared__ intPair queryCellTraj[MAX_QUERY_CELLNUMBER];
	__shared__ intPair dbCellTraj[MAX_QUERY_CELLNUMBER];
	//cellchecked��¼��query�г��ֵ�cell��ţ������ڷ��������ʱ�����ǲ����Ѿ������ˡ��Ժ�����ڹ鲢���и��ô˱�����
	__shared__ int cellChecked[MAX_QUERY_CELLNUMBER];
	for (int i = 0; i <= queryCellLength - 1; i += MAXTHREAD) {
		if (threadID+i < queryCellLength)
		{
			queryCellTraj[threadID + i] = queryFVGPU[threadID + i];
		}
	}
	int dbTrajStartIdx = FVinfo[startTrajIdx + blockID].int_2;
	int dbTrajEndIdx;
	if (blockID + startTrajIdx == trajNumInDB - 1)
		dbTrajEndIdx = nonZeroFVNumInDB - 1;
	else
		dbTrajEndIdx = FVinfo[startTrajIdx + blockID + 1].int_2 - 1;
	
	for (int i = 0; i <= dbTrajEndIdx - dbTrajStartIdx;i+=MAXTHREAD)
	{
		if (threadID + i <= dbTrajEndIdx - dbTrajStartIdx)
			dbCellTraj[threadID + i] = FVTable[dbTrajStartIdx + threadID + i];
	}
	//1.1:��query��ȥdb
	for (int i = 0; i < queryCellLength; i += MAXTHREAD)
	{
		if (threadID + i < queryCellLength) {
			int find = binary_search_intPair(dbCellTraj, 0, dbTrajEndIdx - dbTrajStartIdx, queryCellTraj[threadID + i].int_1);
			cellChecked[threadID + i] = queryCellTraj[threadID + i].int_1;
			SubbedArray[SubbedArrayJump * blockID + threadID + i].int_1 = queryCellTraj[threadID + i].int_1;
			SubbedArray[SubbedArrayJump * blockID + threadID + i].int_2 = queryCellTraj[threadID + i].int_2 - find;
		}
		if (threadID == 0) {
			SubbedArrayOffset[blockID].int_1 = queryCellLength - 1;
			SubbedArrayOffset[blockID].int_2 = queryCellLength + dbTrajEndIdx - dbTrajStartIdx;
		}
	}
	//1.2����db��ȥquery��ע��Ӹ���
	for (int i = 0; i <= dbTrajEndIdx - dbTrajStartIdx;i+=MAXTHREAD)
	{
		if(threadID + i <= dbTrajEndIdx - dbTrajStartIdx)
		{
			intPair cellNo = dbCellTraj[threadID + i];
			int find = binary_search_int(cellChecked, 0, queryCellLength - 1, cellNo.int_1);
			if (find == -1)
			{
				SubbedArray[SubbedArrayJump * blockID + queryCellLength + threadID + i].int_1 = cellNo.int_1;
				SubbedArray[SubbedArrayJump * blockID + queryCellLength + threadID + i].int_2 = -cellNo.int_2;
			}
			else
				SubbedArray[SubbedArrayJump * blockID + queryCellLength + threadID + i].int_1 = -1;
		}
	}
	__syncthreads();
	//�ڶ��׶Σ��������ڣ�������
	//����׶θ�Ϊÿ��thread����һ��FD
	//2.1���ϲ�ÿ��subbedArray
	if (threadIDGlobal < checkNum) {
		int startMergeIdx = SubbedArrayOffset[threadIDGlobal].int_1 + 1;
		int endMergeIdx = SubbedArrayOffset[threadIDGlobal].int_2;
		int frontPtr = startMergeIdx;
		for (int i = startMergeIdx; i <= endMergeIdx;i++)
		{
			if(SubbedArray[SubbedArrayJump * threadIDGlobal + i].int_1 != -1)
			{
				SubbedArray[SubbedArrayJump * threadIDGlobal + frontPtr] = SubbedArray[SubbedArrayJump * threadIDGlobal + i];
				frontPtr++;
			}
		}
		SubbedArrayOffset[threadIDGlobal].int_2 = frontPtr-1;
	}
	//2.2 ��������
	int neighborsID[8];
	//cell����ָ�ڼ���Ԫ��
	for (int cell = 0; cell <= SubbedArrayOffset[threadIDGlobal].int_2; cell++)
	{
		findNeighborGPU(cellNum, cell, neighborsID);
		//for (int i = 0; i <= 7; i++)
		//	neighborsID[i] = 11;
		for (int i = 0; i <= 7; i++)
		{
			int find = binary_search_intPair_Neighbor(&SubbedArray[SubbedArrayJump * threadIDGlobal], 0, SubbedArrayOffset[threadIDGlobal].int_1, neighborsID[i]);
			if(find == -1){
				find = binary_search_intPair_Neighbor(&SubbedArray[SubbedArrayJump * threadIDGlobal], SubbedArrayOffset[threadIDGlobal].int_1 + 1, SubbedArrayOffset[threadIDGlobal].int_2, neighborsID[i]);
			}
			// �����-1��˵�����neighbor��0�����ô���
			if(find != -1)
			{
				if (isPositive(SubbedArray[SubbedArrayJump * threadIDGlobal + cell].int_2) != isPositive(SubbedArray[SubbedArrayJump * threadIDGlobal + find].int_2))
				{
					if (fabsf(SubbedArray[SubbedArrayJump * threadIDGlobal + cell].int_2) > fabsf(SubbedArray[SubbedArrayJump * threadIDGlobal + find].int_2))
					{
						SubbedArray[SubbedArrayJump * threadIDGlobal + cell].int_2 = SubbedArray[SubbedArrayJump * threadIDGlobal + cell].int_2 + SubbedArray[SubbedArrayJump * threadIDGlobal + find].int_2;
						SubbedArray[SubbedArrayJump * threadIDGlobal + find].int_2 = 0;
					}
					else {
						SubbedArray[SubbedArrayJump * threadIDGlobal + find].int_2 = SubbedArray[SubbedArrayJump * threadIDGlobal + find].int_2 + SubbedArray[SubbedArrayJump * threadIDGlobal + cell].int_2;
						SubbedArray[SubbedArrayJump * threadIDGlobal + cell].int_2 = 0;
						break;
					}
				}
			}
		}
	}
	__syncthreads();
	//�����׶Σ�ͳ����������
	//��Ȼ��ÿ��block����һ��FD�ļ���
	if (blockID >= checkNum)
		return;
	int *tempsumPosi = (int*)queryCellTraj;
	int *tempsumNega = (int*)dbCellTraj;
	tempsumPosi[threadID] = 0;
	tempsumNega[threadID] = 0;
	for (int i = 0; i <= SubbedArrayOffset[blockID].int_2; i += MAXTHREAD)
	{
		if(i+threadID <= SubbedArrayOffset[blockID].int_2)
		{
			tempsumPosi[threadID] += (isPositive(SubbedArray[SubbedArrayJump * blockID + i + threadID].int_2)*SubbedArray[SubbedArrayJump * blockID + i + threadID].int_2);
			tempsumNega[threadID] += (-(!isPositive(SubbedArray[SubbedArrayJump * blockID + i + threadID].int_2))*SubbedArray[SubbedArrayJump * blockID + i + threadID].int_2);
		}
	}
	__shared__ int sizeOfTempSum;
	if (threadID == 0)
		sizeOfTempSum = MAXTHREAD;
	__syncthreads();
	while ((sizeOfTempSum>1))
	{
		if (threadID <= (sizeOfTempSum >> 1) - 1)
		{
			tempsumPosi[threadID] = tempsumPosi[threadID] + tempsumPosi[threadID + (sizeOfTempSum >> 1)];
			tempsumNega[threadID] = tempsumNega[threadID] + tempsumNega[threadID + (sizeOfTempSum >> 1)];
		}
		__syncthreads();
		if (threadID == 0)
			sizeOfTempSum = (sizeOfTempSum >> 1);
		__syncthreads();
	}
	if (threadID == 0)
		FDistance[blockID] = (tempsumPosi[0] > tempsumNega[0]) ? tempsumPosi[0] : tempsumNega[0];
}

//ÿ��block����һ��FD�ļ���
__global__ void Calculate_FD_NonColumn(short* queryFVGPU, intPair* FVinfo, intPair* FVTable, int startTrajIdx, int checkNum,int cellNum, int trajNumInDB, int nonZeroFVNumInDB, short* FDistance)
{
	//��һ�׶Σ����м���
	int blockID = blockIdx.x;
	int threadID = threadIdx.x;
	int threadIDGlobal = blockDim.x*blockID + threadID;
	if (blockID >= checkNum)
		return;
	__shared__ intPair taskInfo;
	if(threadID == 0)
		taskInfo = FVinfo[blockID + startTrajIdx];
	int nextCnt;
	if (blockID + startTrajIdx == trajNumInDB - 1)
		nextCnt = nonZeroFVNumInDB;
	else
		nextCnt = FVinfo[blockID + startTrajIdx + 1].int_2;
	__syncthreads();
	for (int i = 0; i <= (cellNum-1);i+=MAXTHREAD)
	{
		int find = binary_search_intPair(FVTable, taskInfo.int_2, (nextCnt - 1), (i + threadID));
		//int find = 1;
		//int k = cellNum*blockID + (i + threadID);
		//queryFVGPU[cellNum*blockID + (i + threadID)] = 2;
		queryFVGPU[cellNum*blockID + (i + threadID)] = queryFVGPU[cellNum*blockID + (i + threadID)] - find;
	}
	//�ڶ��׶Σ��������ڣ�������
	//����׶θ�Ϊÿ��thread����һ��FD
	int neighborsID[8];
	for (int cell = 0; cell <= cellNum - 1;cell++)
	{
		//ֻ��Ҫһ�����߳̾�����
		if (threadIDGlobal >= checkNum)
			break;
		if (queryFVGPU[cellNum*threadIDGlobal + cell] != 0)
		{
			findNeighborGPU(cellNum, cell, neighborsID);
			//for (int i = 0; i <= 7; i++)
			//	neighborsID[i] = 11;
			for (int i = 0; i <= 7; i++)
			{
				if (isPositive(queryFVGPU[cellNum*threadIDGlobal + cell]) != isPositive(queryFVGPU[cellNum*threadIDGlobal + neighborsID[i]])){
					if (fabsf(queryFVGPU[cellNum*threadIDGlobal + cell]) > fabsf(queryFVGPU[cellNum*threadIDGlobal + neighborsID[i]]))
					{
						queryFVGPU[cellNum*threadIDGlobal + cell] = queryFVGPU[cellNum*threadIDGlobal + cell] + queryFVGPU[cellNum*threadIDGlobal + neighborsID[i]];
						queryFVGPU[cellNum*threadIDGlobal + neighborsID[i]] = 0;
					}
					else
					{
						queryFVGPU[cellNum*threadIDGlobal + neighborsID[i]] = queryFVGPU[cellNum*threadIDGlobal + neighborsID[i]] + queryFVGPU[cellNum*threadIDGlobal + cell];
						queryFVGPU[cellNum*threadIDGlobal + cell] = 0;
						break;
					}
				}
			}
		}
	}
	__syncthreads();
	//�����׶Σ�ͳ����������
	//��Ȼ��ÿ��block����һ��FD�ļ���
	__shared__ int tempsumPosi[MAXTHREAD], tempsumNega[MAXTHREAD];
	tempsumPosi[threadID] = 0;
	tempsumNega[threadID] = 0;
	for (int i = 0; i <= cellNum - 1;i+=MAXTHREAD)
	{
		tempsumPosi[threadID] += (isPositive(queryFVGPU[blockID*cellNum + (i + threadID)])*queryFVGPU[blockID*cellNum + (i + threadID)]);
		tempsumNega[threadID] += (-(!isPositive(queryFVGPU[blockID*cellNum + (i + threadID)]))*queryFVGPU[blockID*cellNum + (i + threadID)]);
	}
	__shared__ int sizeOfTempSum;
	if(threadID==0)
		sizeOfTempSum = MAXTHREAD;
	__syncthreads();
	while((sizeOfTempSum>1))
	{
		if (threadID <= (sizeOfTempSum >> 1)-1)
		{
			tempsumPosi[threadID] = tempsumPosi[threadID] + tempsumPosi[threadID + (sizeOfTempSum>>1)];
			tempsumNega[threadID] = tempsumNega[threadID] + tempsumNega[threadID + (sizeOfTempSum>>1)];
		}
		__syncthreads();
		if(threadID == 0)
			sizeOfTempSum = (sizeOfTempSum >> 1);
		__syncthreads();
	}
	if (threadID == 0)
		FDistance[blockID] = (tempsumPosi[0] > tempsumNega[0]) ? tempsumPosi[0] : tempsumNega[0];

}

//SubbedArrayJump��SubbedArray��ÿһ���ж��ٸ�Ԫ�أ�������idx��
int Similarity_Pruning_Handler(intPair* queryFVGPU, intPair* FVinfo, intPair* FVTable, intPair* SubbedArray, intPair* SubbedArrayOffset,int SubbedArrayJump, int queryCellLength, int startTrajIdx, int checkNum, int cellNum, int trajNumInDB, int nonZeroFVNumInDB, short* FDistance, cudaStream_t stream)
{
#ifdef NOT_COLUMN_ORIENTED
	Calculate_FD_NonColumn <<<checkNum, MAXTHREAD, 0, stream >>>(queryFVGPU, FVinfo, FVTable, startTrajIdx, checkNum, cellNum, trajNumInDB, nonZeroFVNumInDB, FDistance);
#else
	Calculate_FD_Sparse <<<checkNum, MAXTHREAD, 0, stream >>>(queryFVGPU, FVinfo, FVTable, SubbedArray, SubbedArrayOffset, SubbedArrayJump, queryCellLength, startTrajIdx, checkNum, cellNum, trajNumInDB, nonZeroFVNumInDB, FDistance);
#endif
	return 0;
}


/*
//�Ȱ����ܷ���һ��SMִ��һ��DP�����������ٷֱ��������kernel
//constructing...
���Ż���
1��queryTra��queryLength����candidateLength����ͨ����ֵ�ķ�ʽֱ�Ӵ��ݵ�SM�ļĴ���������ȫ���ڴ��ʹ��

*/
int handleEDRdistance(SPoint *queryTra, SPoint **candidateTra, int candidateNum, int queryLength, int *candidateLength,int *result) {
	MyTimer time1;
	time1.start();

	int** stateTableGPU=NULL;
	//��GPU��Ϊ״̬������ڴ�
	int** temp=NULL;
	temp = (int**)malloc(sizeof(int*)*candidateNum);
	for (int i = 0; i <= candidateNum - 1; i++) {
		CUDA_CALL(cudaMalloc((void**)&temp[i], sizeof(int)*(candidateLength[i] + 1)*(queryLength + 1)));
	}
	CUDA_CALL(cudaMalloc((void***)&stateTableGPU, sizeof(int*)*candidateNum));
	CUDA_CALL(cudaMemcpy(stateTableGPU, temp, candidateNum*sizeof(int*), cudaMemcpyHostToDevice));

	//Ϊ�洢�Ĺ켣��Ϣ�����ڴ�
	SPoint *queryTraGPU=NULL, **candidateTraGPU=NULL;
	int *candidateLengthGPU=NULL, *resultGPU=NULL;
	CUDA_CALL(cudaMalloc((void**)&queryTraGPU, sizeof(SPoint)*queryLength));
	CUDA_CALL(cudaMalloc((void**)&candidateLengthGPU, sizeof(int)*candidateNum));
	//CUDA_CALL(cudaMalloc((void**)&resultGPU, sizeof(int)*candidateNum));

	SPoint **tempS = (SPoint**)malloc(sizeof(SPoint*)*candidateNum);
	for (int i = 0; i <= candidateNum - 1; i++) {
		CUDA_CALL(cudaMalloc((void**)&tempS[i], sizeof(SPoint)*candidateLength[i]));
		
	}
	CUDA_CALL(cudaMalloc((void***)&candidateTraGPU, sizeof(SPoint*)*candidateNum));
	CUDA_CALL(cudaMemcpy(candidateTraGPU, tempS, candidateNum*sizeof(SPoint*), cudaMemcpyHostToDevice));
	//
	time1.stop();
	std::cout << time1.elapse() << std::endl;
	time1.start();
	//
	//���ͨ���������ķ������ݹ켣�����Ҫ��켣�����洢
	//��GPU���ݹ켣��Ϣ
	CUDA_CALL(cudaMemcpy(queryTraGPU, queryTra, queryLength*sizeof(SPoint), cudaMemcpyHostToDevice));
	CUDA_CALL(cudaMemcpy(candidateLengthGPU, candidateLength, candidateNum*sizeof(int), cudaMemcpyHostToDevice));
	
	for (int i = 0; i <= candidateNum - 1; i++) {
		CUDA_CALL(cudaMemcpy(tempS[i], candidateTra[i], candidateLength[i] * sizeof(SPoint), cudaMemcpyHostToDevice));
	}
	//for (int i = 0; i <= candidateNum - 1;i++)
	//	CUDA_CALL(cudaMemcpy(candidateTraGPU[i], candidateTra[i], candidateLength[i]*sizeof(SPoint), cudaMemcpyHostToDevice));
	CUDA_CALL(cudaHostAlloc((void**)&result, candidateNum*sizeof(int), cudaHostAllocWriteCombined | cudaHostAllocMapped));
	CUDA_CALL(cudaHostGetDevicePointer(&resultGPU, result, 0));
	time1.stop();
	std::cout << time1.elapse() << std::endl;
	time1.start();
	//ִ��kernel
	EDRDistance_1 <<<candidateNum, MAXTHREAD >>>(queryTraGPU, candidateTraGPU, candidateNum, queryLength, candidateLengthGPU, stateTableGPU, resultGPU);

	//ȡ���
	//result = (int*)malloc(candidateNum*sizeof(int));
	//CUDA_CALL(cudaMemcpy(result, resultGPU, candidateNum*sizeof(int), cudaMemcpyDeviceToHost));
	cudaDeviceSynchronize();
//	for (int j = 0; j <= candidateNum - 1;j++)
//		std::cout << result[j] << std::endl;

	//free GPU!!!!!
	time1.stop();
	std::cout << time1.elapse() << std::endl;
	return 0;

}

