#pragma once
#include "ConstDefine.h"
//����CUDA������point��ַ��point��������ѯ��mbb�������ַ���������
#ifdef _CELL_BASED_STORAGE
int cudaRangeQueryHandler(int* candidateCells, int* rangeStarts, int* rangeEnds, int candidateCellNum, float xmin, float ymin, float xmax, float ymax, int*& idxsGPU, int& resultNum, Point *pointsPtrGPU,int *&resultIdx);
int putCellDataSetIntoGPU(Point* pointsPtr, Point*& pointsPtrGPU, int pointNum);
#endif