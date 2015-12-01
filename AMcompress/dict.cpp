#include "rely.h"
#include "basic.h"
#include "checker.h"
#include "dict.h"


namespace acp
{
	_CRT_ALIGN(16) static uint16_t DictSize_Max,//max count of diction
		DictSize_Cur;//current size of diction
	struct DictInfo
	{
		uint16_t benefit;
		uint16_t dnum;
		uint8_t size,
			empty;
	};
	struct DictIndex
	{
		uint8_t index[56];
	};
	class DictJump
	{
	public:
		struct Jump
		{
			uint16_t index;
			uint8_t size,
				empty;
		};
		Jump *index[64];
		int16_t size[64];
		uint8_t tNum;
		void init(const uint16_t dsize, const uint8_t t)
		{
			tNum = t;
			for (uint8_t a = 0; a < tNum; ++a)
			{
				index[a] = (Jump*)malloc_align(dsize*sizeof(Jump) / tNum, 64);
				size[a] = 0;
			}
		}
		void exit()
		{
			for (uint8_t a = 0; a < tNum; ++a)
				free_align(index[a]);
		}
		inline void add(const uint8_t dsize, uint16_t ival = 0xffff)
		{
			uint16_t val = 0;
			val = (val == 0xffff ? DictSize_Cur : ival);
			uint8_t tId = val%tNum;
			index[tId][size[tId]].index = val;
			index[tId][size[tId]].size = dsize;
		}
		inline void del()
		{
			for (uint8_t a = 0; a < tNum; ++a)
				size[a] = 0;
		}
		inline uint16_t get(const uint8_t id, int16_t &pos, uint8_t &outsize)
		{
			pos = 0;
			if (size[id])
			{
				outsize = index[id][0].size;
				return index[id][0].index;
			}
			return 0xffff;
		}
		inline uint16_t chg(const uint8_t id, const uint16_t did, int16_t &pos)
		{
			int16_t left = 0,//left border
				right = size[id] - 1,//right border
				mid = (left + right) / 2;//middle pos

			while (left <= right)
			{
				if (did > index[id][mid].index)
					left = mid + 1;
				else
					right = mid - 1;
				mid = (left + right) / 2;
			}
			pos = left;
			return 0x0;
		}
		inline uint16_t next(const uint8_t id, const uint8_t dsize, int16_t &pos, uint8_t &outsize)
		{
			for (; ++pos < size[id];)
				if (index[id][pos].size >= dsize)
				{
					outsize = index[id][pos].size;
					return index[id][pos].index;
				}
			return 0xffff;
		}
	};

	class TLT
	{
	public:
		uint16_t *dID;
		TLT(uint16_t dsize)
		{
			dID = (uint16_t*)malloc_align(2 * dsize, 64);
		}
		~TLT() { free_align(dID); }
		void add(uint16_t val)
		{
			dID[DictSize_Cur] = val;
			for (uint16_t a = 0; a < DictSize_Cur; ++a)
				if (dID[a] >= val)
					dID[a]++;
		}
		void set()
		{
			for (uint16_t a = 0; a < DictSize_Cur; ++a)
				dID[a] = a;
		}
		void upd(uint16_t oldval, uint16_t newval)
		{
			for (uint16_t a = 0; a < DictSize_Cur; ++a)
			{
				if (dID[a] >= newval && dID[a] < oldval)
					dID[a]++;
				else if (dID[a] == oldval)
					dID[a] = newval;
			}
		}
	};

	mutex mtx_Dict_Use;//
	static mutex mtx_FindThread_Wait,//
		mtx_CtrlThread_Wait;//

	condition_variable cv_Dict_Use,//cv for upper to wake up ctrl thread
		cv_Dict_Ready;//cv to wake up upper thread
	static condition_variable cv_FindThread_Wait,//cv to wake up find thread
		cv_CtrlThread_Wait;//cv to wake up ctrl thread
	static atomic_uint64_t a_FT_state(0);
	static int8_t FindLen;
	static uint32_t a_OnlyID = 0;
	static uint32_t a_chk_times = 0;

	static bool isFirstFree = true;
	static bool isDicPage = true;
	_CRT_ALIGN(16) static DictItem *Diction, *DicA, *DicB;//data of diction
	_CRT_ALIGN(16) static DictInfo *DictList;//list of diction
	_CRT_ALIGN(16) static DictIndex *DictIdx, *DidxA, *DidxB;//index of diction
	_CRT_ALIGN(16) static TLT *DictTLT;
	_CRT_ALIGN(16) static uint64_t judgenum[65];//judge num
	_CRT_ALIGN(16) static int64_t idxjudge[55];//judge num
	_CRT_ALIGN(16) static DictJump *DictJmp[56];


	void Dict_init(uint16_t diccount)
	{
		DicA = (DictItem*)malloc_align(sizeof(DictItem)*diccount, 64);
		DicB = (DictItem*)malloc_align(sizeof(DictItem)*diccount, 64);
		Diction = DicA;
		DictList = (DictInfo*)malloc_align(sizeof(DictInfo)*diccount, 64);
		DidxA = (DictIndex*)malloc_align(sizeof(DictIndex)*diccount, 64);
		DidxB = (DictIndex*)malloc_align(sizeof(DictIndex)*diccount, 64);
		DictIdx = DidxA;
		DictTLT = new TLT(diccount);
		DictSize_Max = diccount;
		DictSize_Cur = 0;
		isFirstFree = true;
		
		for (auto a = 0; a < 56; ++a)
			DictJmp[a] = new DictJump();

		uint8_t judgetmp[8];
		memset(judgetmp, 0, 8);
		memset(judgenum, 0xff, 520);
		for (int a = 0; a < 8; ++a)
		{
			judgenum[a] = *(uint64_t*)judgetmp;
			judgetmp[a] = 0xff;
		}
		int64_t ijtmp = 0x4000000000000000i64;
		for (auto a = 0; a < 53; ijtmp = ijtmp >> 1)
			idxjudge[a++] = ijtmp;

		return;
	}

	void Dict_exit()
	{
#if DEBUG
		dumpdict();
#endif
		free_align(DicA);
		free_align(DicB);
		free_align(DictList);
		free_align(DidxA);
		free_align(DidxB);
		delete DictTLT;
		for (auto a = 0; a < 53; ++a)
			delete DictJmp[a];
		return;
	}

	static inline void DictListSort(uint16_t start, uint16_t end, bool isNew = true)
	{
		DictInfo objd = DictList[end];//dict info of the changed one
		DictIndex objidx = DictIdx[objd.dnum];
		if (end == start)
		{
			if (isNew)
			{
				for (auto a = 0; a < 53; ++a)
					if (objidx.index[a] != 0x7f)
						DictJmp[a]->add(objd.size);
				DictTLT->add(0);
			}
			return;
		}
		int16_t left = start,//left border
			right = end - 1,//right border
			mid;//middle pos
		
		uint16_t obj = objd.benefit;
		//if (DictList[right].benefit > obj)//no need to change
			//return;
		while (left <= right)
		{
			mid = (left + right) / 2;

			if (obj < DictList[mid].benefit)//the one should go right
				left = mid + 1;

			else//DictList[mid].benefit <= obj //the one should go left
				right = mid - 1;
		}
		memmove(&DictList[left + 1], &DictList[left], sizeof(DictInfo)*(end - left));
		DictList[left] = objd;
		
		//
		if (isNew)
		{
			for (auto a = 0; a < 53; ++a)
				if (objidx.index[a] != 0x7f)
					DictJmp[a]->add(objd.size);
			DictTLT->add(left);
		}
		else
		{
			DictTLT->upd(left, end);
		}
		return;
	}

	static inline void DictReBen()
	{
		for (uint16_t a = 0; a < DictSize_Cur; ++a)
		{
			DictList[a].benefit /= 2;
		}
		return;
	}

	static void DictUse(const uint16_t dID, const uint8_t ben)
	{
#if DEBUG_Com
		wchar_t db_str[120];
		swprintf(db_str, L"DIC USE: id=%d\n", dID);
		db_log(1, db_str);
#endif
		DictList[dID].benefit += ben;
		DictListSort(0, dID, false);
		if (DictList[dID].benefit > 0xefff)
			DictReBen();
		return;
	}

	static inline void DictClean()
	{
		uint16_t want = DictSize_Max / 2;
		DictSize_Cur -= want;
		DictTLT->set();
		DictItem *dsrc;
		DictIndex *disrc;
		uint16_t dnum = 0;
		//change page
		if (isDicPage)
		{
			dsrc = DicA;
			Diction = DicB;
			disrc = DidxA;
			DictIdx = DidxB;
			isDicPage = false;
		}
		else
		{
			dsrc = DicB;
			Diction = DicA;
			disrc = DidxB;
			DictIdx = DidxA;
			isDicPage = true;
		}
		//clean dictjmp
		for (uint8_t a = 0; a < 53; ++a)
			DictJmp[a]->del();
		//move data and make dictjmp
		for (uint16_t a = 0; a < DictSize_Cur; ++a)
		{
			dnum = DictList[a].dnum;
			Diction[a] = dsrc[dnum];
			DictIdx[a] = disrc[dnum];
			for (uint8_t b = 0; b < 53; ++b)
				if (DictIdx[a].index[b] != 0x7f)
					DictJmp[b]->add(DictList[a].size, a);
		}
		DictTLT->set();
		return;
	}

	static inline int64_t DictPre(DictItem &dicdata, DictIndex &dicidx, const uint8_t len)
	{
		//memset(dicdata.jump, 0x7f, 64);
		uint8_t *dic_dat, *dic_jmp;
		if (len > 32)
		{
			dic_dat = dicdata.L.data;
			dic_jmp = dicdata.L.jump;
		}
		else
		{
			dic_dat = dicdata.S.data;
			dic_jmp = dicdata.S.jump;
		}
		memset(dicidx.index, 0x7f, sizeof(DictIndex));
		for (uint8_t a = len - 1; a > 1;--a)
		{
			auto tmp = hash(&dic_dat[a]);
			uint8_t dat = tmp % 53;
			dic_jmp[a] = dicidx.index[dat];
			dicidx.index[dat] = a;
		}
		dic_jmp[0] = dic_jmp[1] = 0x7f;
		int64_t tab = 0;
		/*for (uint8_t a = 53; a--;)
		{
			if (dicidx.index[a] != 0x7f)
				tab += idxjudge[a];
		}*/
		return tab;
	}

	void DictAdd(const uint8_t len, const uint8_t data[])
	{
		_mm_prefetch((char*)data, _MM_HINT_NTA);
		if (DictSize_Cur == DictSize_Max)
			DictClean();
		DictInfo &DicInfo = DictList[DictSize_Cur];
		DicInfo.dnum = DictSize_Cur;
		DicInfo.benefit = DicInfo.size = len;
		DictItem &dicdata = Diction[DictSize_Cur];
		//_mm_prefetch((char*)&dicdata, _MM_HINT_T0);

#if DEBUG_Com
		wchar_t db_str[120];
		swprintf(db_str, L"DIC ADD: len=%d\n", DictList[DictSize_Cur].size);
		db_log(1, db_str);
#endif
		
		memset(dicdata.L.data, 0, 64);
		memcpy(dicdata.L.data, data, len);
		DictPre(dicdata, DictIdx[DictSize_Cur], len);
		DictListSort(0, DictSize_Cur);
		++DictSize_Cur;
		return;
	}

	uint8_t DictGet(const uint16_t dID, uint8_t offset, uint8_t &len, uint8_t data[])
	{
		if (dID >= DictSize_Cur)
			return 0x1;
		if (len & 0x80)//middle align, unknown len
			len = DictList[dID].size - (0xff - len) - offset;
		//else if (offset & 0x80)//right align, unknown offset
			//offset = DictList[dID].len - len;
		if (offset + len > DictList[dID].size)
			return 0x2;
		DictItem &dicdata = Diction[DictList[dID].dnum];
		memcpy(data, dicdata.L.data + offset, len);
		DictUse(dID, len);
		return 0x0;
	}

	static void FindThread_x64(const uint8_t tNum, const int8_t tID, uint8_t &op, ChkItem *inchk, DictReport &dicrep)
	{
		ChkItem chkdata;
		DictItem *dicdata;
		DictIndex *dicidx;
		uint8_t *dic_dat,
			*dic_jmp;
		uint64_t *p_dic_cur,//current pos of dic_data
			*p_chk_cur;//current pos of chker_data
		uint16_t dbnum,
			dbnum_next = 0;
		uint8_t dic_size = 0,
			dic_size_next = 0;
		char *p_prefetch;//for prefetch
		uint8_t findpos,
			objpos,//object-bit pos in the dict
			chkleft;//left count of checker

		uint8_t	&chk_minpos = chkdata.minpos;
		uint8_t &chk_minval = chkdata.minval;
		int8_t dicspos,//real start pos of dict
			maxpos;//max find pos(start) in the dict
		int64_t maxpos_next;
		const uint8_t num_add = tNum * 8 - 7;
		int16_t mypos = 0;

		//init
		const uint64_t mask = 0x1Ui64 << tID;
		unique_lock <mutex> lck(mtx_FindThread_Wait);

#if DEBUG_Thr
		wchar_t msg[6][24];

		//prepare msg
		swprintf(msg[0], L"Dic_FThr %2d creat\n", tID);
		swprintf(msg[1], L"Dic_FThr %2d init\n", tID);
		swprintf(msg[2], L"Dic_FThr %2d lock\n", tID);
		swprintf(msg[3], L"Dic_FThr %2d unlock\n", tID);
		swprintf(msg[4], L"DFT %2d noti DC0\n", tID);
		swprintf(msg[5], L"DFT %2d wa<- DC0\n", tID);

		log_thr(msg[0]);
#endif
		a_FT_state -= mask;
		log_thr(msg[4]);

		cv_CtrlThread_Wait.notify_all();
			
		cv_FindThread_Wait.wait(lck, [=] {return a_FT_state & mask; });
		log_thr(msg[5]);

		lck.unlock();
		log_thr(msg[3]);log_thr(msg[1]);

		//prefetch chkdata
		_mm_prefetch((char*)chkdata.data, _MM_HINT_T0);//data
		_mm_prefetch((char*)chkdata.data + 64, _MM_HINT_T0);//propety

		//main part
		auto func_findnext = [&]
		{
			//for (dic_num_next += dic_num_add[dic_add_idx++]; dic_num_next < DictSize_Cur; dic_num_next += dic_num_add[dic_add_idx++])
			//for (; (dic_num_next += dic_num_add[dic_add_idx++]) < DictSize_Cur;)
			/*for (; (dic_num_next += dic_num_add[(dic_add_idx++) & 0xf]) < DictSize_Cur;)
			{
			#if DEBUG_BUF_CHK
				wchar_t db_str[64];
				if (!tID)
				{
					swprintf(db_str, L"\ntry %d ", dic_num_next);
					db_log(2, db_str);
				}
			#endif
				//if (DictList[dic_num_next].tab&idxjudge[chk_minval])
				//if ((DictListA[dic_num_next].tab << (chk_minval + 1)) < 0)
				//judge if len satisfy
					//if ((maxpos_next = DictListA[dic_num_next].size - chkdata.curlen) >= 0)
				
				if ((maxpos_next = (DictListA[dic_num_next].tab&cmp_mask) - cmp_obj) >= 0)
				{
					break;//get it
				}
			}*/
			dbnum_next = DictJmp[chk_minval]->next(tID, chkdata.curlen, mypos, dic_size_next);
			for (; dbnum_next < DictSize_Cur; dbnum_next = DictJmp[chk_minval]->next(tID, chkdata.curlen, mypos, dic_size_next))
			{
				maxpos_next = dic_size_next - chkdata.curlen;
			#if DEBUG_BUF_CHK
				if (!tID)
					db_log(2, L"get");
			#endif
				//prefetch next DictItem
				p_prefetch = (char *)&Diction[dbnum_next];//pos of next object DictItem
				_mm_prefetch(p_prefetch, _MM_HINT_T0);//-data
				p_prefetch = (char *)&DictIdx[dbnum_next].index[chk_minval];//pos of the object DictIndex
				_mm_prefetch(p_prefetch, _MM_HINT_NTA);//-index
				return;
			}

		};
		auto func_tonext = [&]
		{
			dicdata = &Diction[dbnum = dbnum_next];
			dicidx = &DictIdx[dbnum];
			maxpos = (int8_t)maxpos_next;
			if ((dic_size = dic_size_next) > 32)
			{
				dic_dat = dicdata->L.data;
				dic_jmp = dicdata->L.jump;
				_mm_prefetch((char*)dic_jmp, _MM_HINT_T0);//-jump
			}
			else
			{
				dic_dat = dicdata->S.data;
				dic_jmp = dicdata->S.jump;
			}
		};

		while (true)//run one cycle at a FindInDict
		{
			//refresh chker
			memcpy(&chkdata, inchk, sizeof(ChkItem));
			//dic_num_next = dic_num_cur = tID * 8;
			dbnum_next = DictJmp[chk_minval]->get(tID, mypos, dic_size_next);
			if (dbnum_next == 0xffff)//no find
				goto SIG_BACK;

			func_tonext();
			//cmp_mask = idxjudge[chk_minval] + 0xff;
			//cmp_obj = idxjudge[chk_minval] + chkdata.curlen;

			//prefetch current
			p_prefetch = (char *)dicdata;//pos of the object DictItem
			_mm_prefetch(p_prefetch, _MM_HINT_T0);//-data
			//_mm_prefetch(p_prefetch + 64, _MM_HINT_T0);//-jump
			p_prefetch = (char *)dicidx;//pos of the object DictIndex
			_mm_prefetch(p_prefetch + (chk_minval & 0xc0), _MM_HINT_NTA);//-index
			
			maxpos = dic_size - chkdata.curlen;
			func_findnext();

			while(true)//loop when finish a DictItem,fail OR suc(add chk)
			{
				findpos = 0x7f;
				//maxpos = dicinfo->len - chkdata.curlen;//max find pos(start) in the dict
				//maxpos:changed ahead 
				//objpos:must on-change
				objpos = dicidx->index[chk_minval];//object-bit pos in the dict
				/*if (objpos == 0x7f)//no matching word
				{
					if (dic_num_next == 0xffff)//no next
						break;

					func_tonext();
					func_findnext();
					continue;
				}*/
				while (objpos < chk_minpos)
					objpos = dic_jmp[objpos];
				//maxpos must < 64
				//when objpos = 0x7f,dicspos > 64 so dicspos must > maxpos
				//when objpos != 0x7f(<64) need to judge
				if ((dicspos = objpos - chk_minpos) > maxpos)//no enough space to match word
				{
					if (dbnum_next == 0xffff)//no next
						break;

					func_tonext();
					func_findnext();
					continue;
				}

				p_dic_cur = (uint64_t*)(dic_dat + dicspos);
				p_chk_cur = (uint64_t*)chkdata.data;
				chkleft = chkdata.curlen;

				while(true)
				{
					//judge part
					if ( ((*p_dic_cur) ^ (*p_chk_cur)) & judgenum[chkleft])
					{//not match
						objpos = dic_jmp[objpos];//get next pos
						dicspos = objpos - chk_minpos;//get real start pos
						if (dicspos > maxpos)//no enough space to match
							break;
						p_dic_cur = (uint64_t*)(dic_dat + dicspos);
						if (chkleft != chkdata.curlen)
						{//in a match, reset chk pos
							p_chk_cur = (uint64_t*)chkdata.data;
							chkleft = chkdata.curlen;
						}
						continue;
					}//end of not match
					else
					{//match from the beginning
						if (chkleft < 9)
						{//match suc
							findpos = dicspos;
							break;
						}
						else
						{//add more match
							++p_dic_cur, ++p_chk_cur;
							chkleft -= 8;
							continue;
						}
					}//end of match

				}
				//end of dead loop to keep searching in a dict
				if(findpos != 0x7f)//find it
				{
					dicrep.isFind = 0xff;
					dicrep.addr = &dic_dat[findpos];
					dicrep.dicID = DictTLT->dID[dbnum];
					dicrep.diclen = dic_size;
					dicrep.offset = findpos;
					dicrep.objlen = chkdata.curlen;
					if (Chk_inc(chkdata) == 0)
						break;//chkdata is full used
					//add chk suc
					DictJmp[chk_minval]->chg(tID, dbnum_next, mypos);
					//cmp_mask = idxjudge[chk_minval] + 0xff;
					//cmp_obj = idxjudge[chk_minval] + chkdata.curlen;
					if (--maxpos_next < 0)//next fail
						func_findnext();
					if (--maxpos < 0)
					{//current failed
						if (dbnum_next == 0xffff)//no next
							break;
						
						func_tonext();
						func_findnext();
						continue;
					}
				}
				else//not find it
				{
					if (dbnum_next == 0xffff)//no next
						break;
					
					func_tonext();
					func_findnext();
					continue;
				}
			}
			//end of seaching the whole diction
		SIG_BACK:
			//send back signal to the control thread
			a_FT_state -= mask;
			lck.lock();
			log_thr(msg[2]);

			if (dicrep.isFind && FindLen < dicrep.objlen)
				FindLen = dicrep.objlen;
			log_thr(msg[4]);

			cv_CtrlThread_Wait.notify_all();
			cv_FindThread_Wait.wait(lck, [&mask] {return a_FT_state & mask; });
			log_thr(msg[5]);

			//waked up from ctrl thread
			lck.unlock();
			log_thr(msg[3]);

			if(op == 0xff)
				break;//break to stop the thread
		}
		a_FT_state -= mask;
		cv_CtrlThread_Wait.notify_all();
		log_thr(msg[4]);

		return;
	}

	void FindInDict(const int8_t tCount, DictOP &op, DictReport drep[], ChkItem &chkdata)
	{
		thread t_find[64];
		uint8_t ftop = 0x0;//op of FindThread
		const uint64_t mask = 0xffffffffffffffffUi64 << tCount;

		unique_lock <mutex> lck_DictUse(mtx_Dict_Use);
		unique_lock <mutex> lck_FindThread(mtx_FindThread_Wait);
		//init
		for (auto a = 0; a < 53; ++a)
			DictJmp[a]->init(DictSize_Max, tCount);
		for (int8_t a = 0; a < tCount; a++)
			t_find[a] = thread(FindThread_x64, tCount, a, ref(ftop), &chkdata, ref(drep[a]));

		a_FT_state = 0xffffffffffffffffUi64;

		for (int8_t a = 0; a < tCount; a++)
			t_find[a].detach();
		
		//wait for init of findthread
		cv_CtrlThread_Wait.wait(lck_FindThread, [&mask] {return a_FT_state.load() == mask; });
		//FindThread init finish
#if DEBUG
		wchar_t db_str[120];
#endif
		log_thr(L"Init ok %d Dic_Fthr.\n");

		//notify upper thread
		op.op = 0;
		cv_Dict_Ready.notify_all();
		log_thr(L"DC0 noti M**\n");

		//give up the mutex to wake up upper thread
		cv_Dict_Use.wait(lck_DictUse, [&] {return op.op != 0; });
		log_thr(L"DC0 wa<- M**\n");

		//start into the main part
		while (true)
		{
			for (int8_t a = 0; a < tCount; a++)
				drep[a].isFind = 0;
			FindLen = 0;
			ftop = 0;
			a_FT_state = 0xffffffffffffffffUi64;

			//wake up all find-thread
			cv_FindThread_Wait.notify_all();
			log_thr(L"DC0 noti DTa\n");

			cv_CtrlThread_Wait.wait(lck_FindThread, [&mask] { return a_FT_state.load() == mask; });
			log_thr(L"DC0 wa<- DTa\n");

			//waked up from find-thread

			op.findlen = FindLen;
			op.op = 0x7f;

			cv_Dict_Ready.notify_all();
			log_thr(L"DC0 noti M**\n");

			cv_Dict_Use.wait(lck_DictUse, [&] {return op.op != 0x7f; });
			log_thr(L"DC0 wa<- M**\n");

			//waked up from upper-thread

			if (op.op == 0xfd)//use dict
			{
				DictUse(op.dID, op.len);
			}
			else if (op.op == 0xfe)//add dict
			{
#if DEBUG_Com
				swprintf(db_str, L"DIC ADD~ from %d\n", op.bOffset);
				db_log(1, db_str);
#endif
				DictAdd(op.len, op.data);
#if DEBUG_Com
				/*
				if (a_OnlyID == op.ollimit + 1)
				{
					swprintf(db_str, L"Prt Buf size=%d\n", op.s_b);
					db_log(4, db_str);
					db_dump(1, op.p_b, op.s_b);
					dumpdict();
				}
				*/
#endif
			}
			else if (op.op == 0xff)//find finish
				break;
				
		}
		//end of finding

		ftop = 0xff;
		a_FT_state = 0xffffffffffffffffUi64;
		cv_FindThread_Wait.notify_all();//wake up all find-thread
		log_thr(L"DC0 noti DTa\n");

		cv_CtrlThread_Wait.wait(lck_FindThread, [&mask] {return a_FT_state == mask; });
		log_thr(L"DC0 wa<- DTa\n");

		//all find-thread finish
		op.op = 0x7f;
		cv_Dict_Ready.notify_all();
		log_thr(L"DC0 noti M**\n");

		lck_DictUse.unlock();
		for (auto a = 0; a < 53; ++a)
			DictJmp[a]->exit();
		return;
	}

	uint8_t getDictLen(uint16_t dID)
	{
		return DictList[dID].size;
	}


	void dumpdict()
	{
		wchar_t db_str[120];
		swprintf(db_str, L"Prt Dic count=%d\n", DictSize_Cur);
		db_log(4, db_str);
		for (auto a = 0; a < DictSize_Cur; ++a)
		{
			swprintf(db_str, L"@Dic%5d oID=%d len=%d ben=%d\n", a,0x0/*DictList[a].oID*/,DictList[a].size,DictList[a].benefit);
			db_log(4, db_str);
		}
		db_dump(2, (uint8_t*)Diction, DictSize_Max*sizeof(DictItem));
	}

	

}



