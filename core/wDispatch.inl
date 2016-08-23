
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

template<typename T,typename IDX>
bool wDispatch<T,IDX>::Register(string className, IDX ActIdx, struct Func_t vFunc)
{
	vector<struct Func_t> vf;
	typename map<string, vector<struct Func_t> >::iterator mp = mProc.find(className);
	if(mp != mProc.end())
	{
		vf = mp->second;
		mProc.erase(mp);

		typename vector<struct Func_t>::iterator itvf = vf.begin();
		for(; itvf != vf.end() ; itvf++)
		{
			if(itvf->mActIdx == ActIdx)
			{
				itvf = vf.erase(itvf);
				itvf--;
			}
		}
	}
	vf.push_back(vFunc);
	//pair<map<string, vector<struct Func_t> >::iterator, bool> itRet;
	//itRet = 
	mProc.insert(pair<string, vector<struct Func_t> >(className, vf));
	mProcNum++;
	return true;
	//return itRet.second;
}

template<typename T,typename IDX>
struct wDispatch<T,IDX>::Func_t* wDispatch<T,IDX>::GetFuncT(string className, IDX ActIdx)
{
	typename map<string, vector<struct Func_t> >::iterator mp = mProc.find(className);
	if(mp != mProc.end())
	{
		typename vector<struct Func_t>::iterator itvf = mp->second.begin();
		for(; itvf != mp->second.end() ; itvf++)
		{
			if(itvf->mActIdx == ActIdx)
			{
				return &*itvf;
			}
		}
	}
	return 0;
}
