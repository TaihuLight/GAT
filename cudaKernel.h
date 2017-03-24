#pragma once
#include "ConstDefine.h"
//����CUDA������point��ַ��point��������ѯ��mbb�������ַ���������
#ifdef _CELL_BASED_STORAGE
int cudaRangeQueryHandler(int* candidateCells, int* rangeStarts, int* rangeEnds, int candidateCellNum, float xmin, float ymin, float xmax, float ymax, Point*& resultsGPU, int& resultNum, Point *pointsPtrGPU, Point *&result);
int putCellDataSetIntoGPU(Point* pointsPtr, Point*& pointsPtrGPU, int pointNum);
#endif