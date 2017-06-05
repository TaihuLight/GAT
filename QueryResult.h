#pragma once

struct CPURangeQueryResult
{
	int traid;
	float x;
	float y;
	CPURangeQueryResult *next;
};

typedef struct RangeQueryResultGPU {
	short jobID;
	short idx; //��candidate�еڼ������Ա㴫��ȥ�����
}RangeQueryResultGPU;
class QueryResult
{
public:
	QueryResult();
	~QueryResult();

	CPURangeQueryResult* start;
};

