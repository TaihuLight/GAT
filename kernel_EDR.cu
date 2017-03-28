//����EDR��GPU�����㷨
//zbw0046 3.22



#include "cuda_runtime.h"
#include "device_launch_parameters.h"
#include <iostream>
#include <stdio.h>
#include "ConstDefine.h"
#include "cudaKernel.h"
#include <assert.h>
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
				if (((p1.x - p2.x)*(p1.x - p2.x) + (p1.y - p2.y)*(p1.y - p2.y)) < EPSILON) {
					subcost = 0;
				}
				else
					subcost = 1;
				int state_ismatch = state[0][threadID] + subcost;
				int state_up = state[1][threadID] + 1;
				int state_left = state[1][threadID+1] + 1;
				if (state_ismatch < state_up)
					myState = state_ismatch;
				else if (state_left < state_up)
					myState = state_left;
				else
					myState = state_ismatch;
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
					myState = state_ismatch;
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
					myState = state_ismatch;
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

