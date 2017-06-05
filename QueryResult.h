#pragma once

struct CPURangeQueryResult
{
	int traid;
	float x;
	float y;
	CPURangeQueryResult *next;
};

typedef struct RangeQueryResultGPU {
	int jobID;
	int idx; //��candidate�еڼ������Ա㴫��ȥ�����
}RangeQueryResultGPU;
class QueryResult
{
public:
	QueryResult();
	~QueryResult();

	CPURangeQueryResult* start;
};

