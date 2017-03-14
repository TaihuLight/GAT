#ifndef TRAJECTORY_H
#define TRAJECTORY_H
#include "ConstDefine.h"
#include "SamplePoint.h"


// �켣�࣬��¼�ù켣�Ĳ��������Ϣ���1024
class Trajectory
{
    public:
        Trajectory();
        Trajectory(int tid,std::string vid);
        int addSamplePoints(float lon,float lat,int time);
        virtual ~Trajectory();
        int tid;
        SamplePoint points[MAXLENGTH]; //�±��0��ʼ
        int length = 0;
        std::string vid;
        int errCounter = 0;
        SamplePoint errPointBuff[10]; //����

    protected:

    private:
};

#endif // TRAJECTORY_H
