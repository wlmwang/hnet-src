
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _SVR_H_
#define _SVR_H_

#include "wCore.h"
#include "wMisc.h"

#define MAX_SVR_HOST 16
#define MAX_SVR_NUM 255			//每次请求svr最多个数

#define SVR_TM_MAX 20
#define INIT_WEIGHT 100      //默认权重值 
#define MAX_WEIGHT 1000     //最大权重值
#define DELAY_MAX 100000000	//最大延时值 100s

enum QOS_RTN
{
	QOS_OVERLOAD	= -10000,	//过载
	QOS_TIMEOUT     = -9999,    //超时
	QOS_SYSERR      = -9998    //系统错误
};

#pragma pack(1)

/*Svr 基础属性 && 通信结构*/
struct SvrNet_t
{
	int		mGid {0};
	int		mXid {0};
	int 	mWeight {INIT_WEIGHT};	//静态权重，0为禁用此Svr
	short	mVersion {0};
	int		mPort {0};
	char	mHost[MAX_SVR_HOST] {'\0'};

    int 	mPre {0};		//预取数
    int 	mExpired {0};	//过期时间

    SvrNet_t() {}

	SvrNet_t(const SvrNet_t &stSvr)
	{
		mGid = stSvr.mGid;
		mXid = stSvr.mXid;
		mWeight = stSvr.mWeight;
		mPort = stSvr.mPort;
		mVersion = stSvr.mVersion;
		mPre = stSvr.mPre;
		mExpired = stSvr.mExpired;
		memcpy(mHost, stSvr.mHost, MAX_SVR_HOST);
	}

	SvrNet_t& operator=(const SvrNet_t &stSvr)
	{
		mGid = stSvr.mGid;
		mXid = stSvr.mXid;
		mWeight = stSvr.mWeight;
		mPort = stSvr.mPort;
		mVersion = stSvr.mVersion;
		mPre = stSvr.mPre;
		mExpired = stSvr.mExpired;
		memcpy(mHost, stSvr.mHost, MAX_SVR_HOST);
		return *this;
	}

	/** 忽略 weight version 比较、排序*/
    bool operator==(SvrNet_t const &other) const
    {
        if (mGid != other.mGid)
        {
            return false;
        }
        else if(mXid != other.mXid)
        {
            return false;
        }
        else if(strcmp(mHost,other.mHost))
        {
            return false;
        }
        else if (mPort != other.mPort)
        {
            return false;
        }
        return true;
    }

	bool operator<(const SvrNet_t &stSvr) const
	{
        if (mGid < stSvr.mGid )
        {
            return true;
        }
        else if(mGid > stSvr.mGid)
        {
        	return false;
        }
        else if(mXid < stSvr.mXid)
        {
            return true;
        }
        else if(mXid > stSvr.mXid)
        {
            return false;
        }
        else if(mPort < stSvr.mPort)
        {
            return true;
        }
        else if(mPort > stSvr.mPort)
        {
            return false;
        }
        else
        {
            int cmp = strcmp(mHost, stSvr.mHost);
            if(cmp < 0)
        	{
				return true;
        	}
            else if(cmp > 0)
            {
            	return false;
            }
        }
        return false;
	}
};

struct SvrCaller_t
{
	int 	mCallerGid {0};
	int     mCallerXid {0};
	int		mCalledGid {0};			//被调模块编码
	int		mCalledXid {0};			//被调接口编码
	char	mHost[MAX_SVR_HOST] {'\0'};//被调主机IP
	unsigned short mPort {0};		//被调主机PORT
	
	int mReqRet {0};				//请求结果。 >=0 成功
	int mReqCount {0};				//请求次数
	long long mReqUsetimeUsec {0};	//微妙
	int mTid {0};					//进程id（为实现）

    bool operator==(SvrCaller_t const &other) const
    {
        if(mCallerGid != other.mCallerGid)
        {
            return false;
        }
        else if(mCallerXid != other.mCallerXid)
        {
            return false;
        }
        if (mCalledGid != other.mCalledGid)
        {
            return false;
        }
        else if(mCalledXid != other.mCalledXid)
        {
            return false;
        }
        else if(strcmp(mHost, other.mHost))
        {
            return false;
        }
        else if(mPort != other.mPort)
        {
            return false;
        }
        return true;
    }

};

#pragma pack()

//访问量的配置信息
struct SvrReqCfg_t
{
	int			mReqLimit {0};			//访问量控制的阀值
	int			mReqMax {0};			//访问量控制的最大值
	int			mReqMin {0};			//访问量控制的最小值
	int			mReqCount {0};			//访问量控制的实际值（请求数）
	float		mReqErrMin {0.0};		//错误的最小阀值 0-1 [小于则服务无错，应增大访问量。大于则服务过载，应减少访问量。]
	float		mReqExtendRate {0.0};	//无错误的时候的访问量阀值扩张率 0.001-101
	int         mRebuildTm {3};        	//统计的周期 60s
	int 		mPreTime {0};			//4(不能大于route重建时间的一半) 可以设定预取时间长度N秒内的路由计数，N小于当前周期的1/2
};

//并发量的配置信息
struct SvrListCfg_t
{
	int			mListLimit {0};		//并发量控制的阀值
	int			mListMax {0};		//并发量控制的最大值 400
	int			mListMin {0};		//并发量控制的最小值 10
	int			mListCount {0};		//并发量控制的实际值
	float		mListErrMin {0.0};	//并发的最小阀值[小于这个值认为是无错] 0.5
	float		mListExtendRate {0.0};//并发无错误的时候的阀值扩张率 0.2
};

//宕机检测和探测的相关配置
struct SvrDownCfg_t
{
	int mReqCountTrigerProbe {0};   //100000
	int mDownTimeTrigerProbe {0};   //600
	int mProbeTimes {0};            //(3,~)
	int mPossibleDownErrReq {0};    //10	连续错误阈值
	float mPossbileDownErrRate {0.0}; //0.5   (0.01,1)	宕机错误率阈值

    //mProbeBegin >0 才打开自探测
    int mProbeBegin {0};    //0
    int mProbeInterval {0}; //3~10
    int mProbeNodeExpireTime {0};//3~600
};

/** 时间段配置 */
struct SvrTM
{
	int mCfgCount {0};					//配置的个数
	int mBeginUsec[SVR_TM_MAX] {0};		//返回时间段开始时间
	int mEndUsec[SVR_TM_MAX] {0};		//返回时间段结束时间
	int mRet[SVR_TM_MAX] {0};			//返回值[0:成功 -1:失败]

	SvrTM(const SvrTM & stm);
};

/** 访问统计 */
struct SvrInfo_t
{
	struct timeval mBuildTm;		//统计信息开始时间, 每个节点 rebuild 时刻的绝对时间
	int			mReqAll {0};			//总的请求数，理论上请求调用agent获取路由次数
	int			mReqRej {0};			//被拒绝的请求数
	int			mReqSuc {0};			//成功的请求数
	int			mReqErrRet {0};			//失败的请求数
	int			mReqErrTm {0};			//超时的请求数

	float       mLoadX {1.0};				//负载总乘数 (mOkLoad*mDelayLoad)*(max<mWeight>)/mWeight  系数
	float       mOkLoad {1.0};			//成功率乘数
	float       mDelayLoad {1.0};			//时延乘数
    float       mDelayLoadAmplify {0.0};	//时延放大
	float       mOkRate {1.0};			//上一周期成功率 0-1
	unsigned int mAvgTm {1};			//上一周期成功请求平均时延，微秒  mTotalUsec/mReqSuc
	long long   mTotalUsec {1}; 		//请求总微秒数
	float 		mAvgErrRate {0.0};		//平均错误率，统计多个周期的错误率并加以平均得到（未超过最低阈值(mReqCfg.mReqErrMin)，始终为0）

	/** 上一周期统计数据 */
	int 		mLastReqAll {0};
	int 		mLastReqRej {0};
	int 		mLastReqErrRet {0};
	int 		mLastReqErrTm {0};
	int  		mLastReqSuc {0};
	bool  		mLastErr {false};		//门限扩张标识 true：收缩  false：扩张
	int 		mLastAlarmReq {0};		//参考值。请求数扩张门限（上一周期数量），判断扩展是否有效
	int 		mLastAlarmSucReq {0};	//参考值。成功请求数扩张门限
	int 		mPreAll {0};			//路由被分配次数 + 预测本周期成功请求次数

	int 		mCityId {0};	//被调所属城市id
	int 		mOffSide {0};	//被调节点与主调异地标志，默认为0， 1标为异地

	int 		mContErrCount {0};		//连续失败次数累积

	int			mSReqAll {0};			//总的请求数(统计用)
	int			mSReqRej {0};			//被拒绝的请求数(统计用)
	int			mSReqSuc {0};			//成功的请求数(统计用)
	int			mSReqErrRet {0};		//失败的请求数(统计用)
	int			mSReqRrrTm {0};			//超时的请求数(统计用)
    int         mSPreAll {0};
    
    int 		mAddSuc {0};			//上个周期与上上个周期成功请求数差值
    int 		mIdle {0};				//add连续核算次数
    
    SvrInfo_t()
    {
		mBuildTm.tv_sec = mBuildTm.tv_usec = 0;
    }

    void InitInfo(struct SvrNet_t& stSvr) {}
};

/**
 * svr阈值（便于自行自行扩张收缩）、统计结构
 */
struct SvrStat_t
{
	int 			mType {0};
	struct SvrReqCfg_t		mReqCfg;	//访问量配置
	struct SvrListCfg_t		mListCfg;	//并发量配置
	struct SvrInfo_t		mInfo;		//统计信息

	//mlist并发量
	//mreq按各个时间段统计信息
	
	void Reset()
	{
		mInfo.mReqAll = 0;
		mInfo.mReqSuc = 0;
		mInfo.mReqRej = 0;
		mInfo.mReqErrRet = 0;
		mInfo.mReqErrTm = 0;
		mInfo.mTotalUsec = 0;
		mInfo.mPreAll = 0;
		mInfo.mContErrCount = 0;
	}

	void AddStatistic()
	{
		mInfo.mSReqAll += mInfo.mReqAll;
		mInfo.mSReqSuc += mInfo.mReqSuc;
		mInfo.mSReqRej += mInfo.mReqRej;
		mInfo.mSReqErrRet += mInfo.mReqErrRet;
		mInfo.mLastReqErrTm += mInfo.mReqErrTm;
		mInfo.mSPreAll += mInfo.mPreAll;
	}

	void ResetStatistic()
	{
		mInfo.mSReqAll = 0;
		mInfo.mSReqSuc = 0;
		mInfo.mSReqRej = 0;
		mInfo.mSReqErrRet = 0;
		mInfo.mLastReqErrTm = 0;
		mInfo.mSPreAll = 0;
	}
};

/**
 * 路由分类
 * 由mGid、mXid组成的一类svr（便于分类路由）
 */
struct SvrKind_t
{
	int		mGid {0};
	int		mXid {0};
	int 	mOverload {0};
    float 	mPtotalErrRate {0.0};		//累计连续过载，所有路由错误率平均值总和
    int 	mPsubCycCount {0};		//累计连续过载次数

    int 	mPtm {0}; 			//rebuild 时刻的绝对时间 time_t
    int 	mRebuildTm {3}; 	//rebuild 的时间间隔
	float 	mWeightSum {0};
	int64_t mAccess64tm {0};	//最近访问时间 微妙

	int mPindex {0};
	
	SvrKind_t()
	{
        mPtm = time(NULL);
        mAccess64tm = GetTimeofday();
	}
	
	SvrKind_t(const SvrKind_t& stKind)
	{
		mGid = stKind.mGid;
		mXid = stKind.mXid;
        mPtotalErrRate = stKind.mPtotalErrRate;
        mPsubCycCount = stKind.mPsubCycCount;
        mRebuildTm = stKind.mRebuildTm;
        mWeightSum = stKind.mWeightSum;
        mOverload = stKind.mOverload;
        mPtm = stKind.mPtm;
        mAccess64tm = stKind.mAccess64tm;
        mPindex = stKind.mPindex;
	}

	SvrKind_t(const SvrNet_t& stNode)
	{
		mGid = stNode.mGid;
		mXid = stNode.mXid;
        mPtm = time(NULL);
        mAccess64tm = GetTimeofday();
	}

	SvrKind_t& operator=(const SvrKind_t& stKind)
	{
		mGid = stKind.mGid;
		mXid = stKind.mXid;
        mPtotalErrRate = stKind.mPtotalErrRate;
        mPsubCycCount = stKind.mPsubCycCount;
        mRebuildTm = stKind.mRebuildTm;
        mWeightSum = stKind.mWeightSum;
        mOverload = stKind.mOverload;
        mPtm = stKind.mPtm;
        mAccess64tm = stKind.mAccess64tm;
        mPindex = stKind.mPindex;
        return *this;
	}

    bool operator<(SvrKind_t const &other) const
    {
        if (mGid < other.mGid)
        {
            return true;
        }
        else if(mGid > other.mGid)
        {
            return false;
        }
        else if(mXid < other.mXid)
        {
            return true;
        }
        else if (mXid > other.mXid)
        {
            return false;
        }
        return false;
    }

    bool operator==(SvrKind_t const &other) const
    {
        if (mGid != other.mGid)
        {
            return false;
        }
        else if(mXid != other.mXid)
        {
            return false;
        }
        return true;
    }
};

/**
 * Svr节点信息
 * 路由信息、各种阈值及统计信息
 */
struct SvrNode_t
{
	SvrNet_t mNet;
	SvrStat_t *mStat {NULL};

	float mKey {0};				//关键值，初始化为 mInfo.mLoadX = 1

	int mStopTime {0};			//宕机记录信息
	int mReqAllAfterDown {0};	//宕机以来所有请求数量
    
    /** 宕机相关的额外恢复条件 */
    int mDownTimeTrigerProbeEx {0};		//时间
    int mReqCountTrigerProbeEx {0};		//请求数量
    
    bool mIsDetecting {false}; 		//是否处在 "探测宕机是否恢复" 的状态  
    //int mHasDumpStatistic {0};//是否备份

    SvrNode_t() {}
    
	SvrNode_t(const struct SvrNet_t& nt, struct SvrStat_t* pStat)
	{
        if (pStat == NULL)
        {
            mKey = 1;
        }
        else
        {
            mKey = pStat->mInfo.mLoadX;
        }
        mNet = nt;
        mStat = pStat;
	}
};

/**
 * sort用到（降序）
 */
inline bool GreaterSvr(const struct SvrNet_t* pR1, const struct SvrNet_t* pR2)
{
	return   pR1->mWeight > pR2->mWeight;
}

#endif
