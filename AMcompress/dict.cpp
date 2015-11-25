#include "rely.h"
#include "basic.h"
#include "checker.h"
#include "dict.h"


namespace acp
{
	struct DictInfo
	{
		uint8_t len;
		uint8_t oID;
		uint16_t benefit;
		//uint32_t oID;
		//DictItem *pos;
		uint16_t dnum;
		uint8_t empty[2];
		uint64_t tab;
	};

	struct DictIndex
	{
		uint8_t index[56];
	};

	mutex mtx_Dict_Use;//
	static mutex mtx_FindThread_Wait,//
		mtx_CtrlThread_Wait;//

	condition_variable cv_Dict_Use,//cv for upper to wake up ctrl thread
		cv_Dict_Ready;//cv to wake up upper thread
	static condition_variable cv_FindThread_Wait,//cv to wake up find thread
		cv_CtrlThread_Wait;//cv to wake up ctrl thread
	static atomic_int64_t a_FT_state(0);
	static int8_t FindLen;
	static uint32_t a_OnlyID = 0;
	static uint32_t a_chk_times = 0;

	static bool isFirstFree = true;
	_CRT_ALIGN(16) static DictItem *Diction;//data of diction
	_CRT_ALIGN(16) static DictInfo *DictList;//list of diction
	_CRT_ALIGN(16) static DictIndex *DictIdx;//index of diction
	_CRT_ALIGN(16) static uint16_t DictSize_Max,//max count of diction
		DictSize_Cur;//current size of diction
	_CRT_ALIGN(16) static uint64_t judgenum[65];//judge num
	_CRT_ALIGN(16) static uint64_t idxjudge[55];//judge num


	bool check()
	{
		a_chk_times++;
		wprintf(L"check() sizeof=%zd\n",sizeof(DictInfo));
		for (auto a = 0; a <= DictSize_Cur; ++a)
		{
			wprintf(L"## %5d oID=%5d len=%2d ben=%3d\n", a, DictList[a].oID, DictList[a].len, DictList[a].benefit);
		}
		wprintf(L"~~~check()\n");
		for (auto a = 1; a < DictSize_Cur; ++a)
		{
			if (DictList[a].benefit > DictList[a - 1].benefit)
			{
				wprintf(L"\nwrong order when %d & size=%d\n", a_chk_times, DictSize_Cur);
				return false;
			}
		}
		return true;
	}


	void Dict_init(uint16_t diccount)
	{
		Diction = (DictItem*)malloc_align(sizeof(DictItem)*diccount, 64);
		//Diction = new DictItem[diccount];
		DictList = (DictInfo*)malloc_align(sizeof(DictInfo)*diccount, 64);
		//DictList = new DictInfo[diccount];
		DictIdx = (DictIndex*)malloc_align(sizeof(DictIndex)*diccount, 64);
		DictSize_Max = diccount;
		DictSize_Cur = 0;
		isFirstFree = true;

		uint8_t judgetmp[8];
		memset(judgetmp, 0, 8);
		memset(judgenum, 0xff, 520);
		for (int a = 0; a < 8; ++a)
		{
			judgenum[a] = *(uint64_t*)judgetmp;
			judgetmp[a] = 0xff;
		}
		uint64_t ijtmp = 1;
		for (auto a = 0; a < 53; ++a, ijtmp = ijtmp << 1)
			idxjudge[a] = ijtmp;

		return;
	}

	void Dict_exit()
	{
#if DEBUG
		dumpdict();
#endif
		free_align(Diction);
		//delete[] Diction;
		free_align(DictList);
		//delete[] DictList;
		free_align(DictIdx);
		return;
	}

	static inline void DictListSort(uint16_t start,uint16_t end)
	{
		//check();
		if (end == start)
			return;
		int32_t left = start,//left border
			right = end - 1,//right border
			mid;//middle pos
		DictInfo objd = DictList[end];//dict info of the changed one
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
		//check();
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
		swprintf(db_str, L"DIC USE: id=%d oID=%d\n", dID, DictList[dID].oID);
		db_log(1, db_str);
#endif
		DictList[dID].benefit += ben;
		//if (DictList[dID].benefit == 0x8)
			//printf("\nhello\n");
		DictListSort(0, dID);
		if (DictList[dID].benefit > 0xefff)
			DictReBen();
		return;
	}

	static inline void DictClean()
	{
		uint16_t want = DictSize_Max / 2;
		DictSize_Cur -= want;
#if DEBUG_Com
		wchar_t db_str[120];
		swprintf(db_str, L"DIC CLE: del=%d iFF=%s\n", want, isFirstFree ? L"true" : L"false");
		db_log(1, db_str);
#endif
		if (isFirstFree)
			isFirstFree = false;
		return;
	}

	static inline uint64_t DictPre(DictItem &dicdata, DictIndex &dicidx, const uint8_t len)
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
			//uint16_t tmpdat = (uint16_t)dicdata.data[a - 1] * 13 + (uint16_t)dicdata.data[a - 2] * 169 + dicdata.data[a];
			auto tmp = hash(&dic_dat[a]);
			uint8_t dat = tmp % 53;
			dic_jmp[a] = dicidx.index[dat];
			dicidx.index[dat] = a;
		}
		dic_jmp[0] = dic_jmp[1] = 0x7f;
		uint64_t tab = 0,
			tmptab = 0x1Ui64 << 52;
		for (uint8_t a = 53; a--;)
		{
			if (dicidx.index[a] != 0x7f)
				tab += idxjudge[a];
		}
		return tab;
	}

	void DictAdd(const uint8_t len, const uint8_t data[])
	{
		_mm_prefetch((char*)data, _MM_HINT_NTA);
		if (DictSize_Cur == DictSize_Max)
			DictClean();
		if (isFirstFree)//fitst time, assign a addr
		{
			//DictList[DictSize_Cur].dpos = DictSize_Cur;
			DictList[DictSize_Cur].dnum = DictSize_Cur;
		}
		DictItem &dicdata = Diction[DictList[DictSize_Cur].dnum];
		_mm_prefetch((char*)&dicdata, _MM_HINT_T0);
		DictList[DictSize_Cur].benefit = DictList[DictSize_Cur].len = len;
		//DictList[DictSize_Cur].oID = a_OnlyID++;
#if DEBUG_Com
		wchar_t db_str[120];
		swprintf(db_str, L"DIC ADD: oID=%d len=%d\n", DictList[DictSize_Cur].oID, DictList[DictSize_Cur].len);
		db_log(1, db_str);
#endif
		
		memset(dicdata.L.data, 0, 64);
		memcpy(dicdata.L.data, data, len);
		DictList[DictSize_Cur].tab = DictPre(dicdata, DictIdx[DictList[DictSize_Cur].dnum], len);
		DictListSort(0, DictSize_Cur);
		++DictSize_Cur;
		return;
	}

	uint8_t DictGet(const uint16_t dID, uint8_t offset, uint8_t &len, uint8_t data[])
	{
		if (dID >= DictSize_Cur)
			return 0x1;
		if (len & 0x80)//middle align, unknown len
			len = DictList[dID].len - (0xff - len) - offset;
		//else if (offset & 0x80)//right align, unknown offset
			//offset = DictList[dID].len - len;
		if (offset + len > DictList[dID].len)
			return 0x2;
		//DictItem &tmp = Diction[DictList[DictSize_Cur].dpos];
		//memcpy(data, tmp.data + offset, len);
		DictItem &dicdata = Diction[DictList[dID].dnum];
		memcpy(data, dicdata.L.data + offset, len);
		DictUse(dID, len);
		return 0x0;
	}

	static void FindThread_x64(const uint8_t tNum, const int8_t tID, uint8_t &op, ChkItem *inchk, DictReport &dicrep)
	{
		ChkItem chkdata;
		DictItem *dicdata;
		DictInfo *dicinfo;
		DictIndex *dicidx;
		uint8_t *dic_dat,
			*dic_jmp;
		uint64_t *p_dic_cur,//current pos of dic_data
			*p_chk_cur;//current pos of chker_data
		uint16_t dic_num_cur,
			dic_num_next = 0;
		char *p_prefetch;//for prefetch
		uint8_t findpos,
			objpos,//object-bit pos in the dict
			chkleft;//left count of checker

		//uint8_t &chk_minval = chkdata.minval,
			//&chk_minpos = chkdata.minpos;
		//uint16_t &chk_minvalD = chkdata.minvalD;
		uint8_t	&chk_minpos = chkdata.minpos;
		uint8_t &chk_minval = chkdata.minval;
		int8_t dicspos,//real start pos of dict
			maxpos,//max find pos(start) in the dict
			maxpos_next;
		uint8_t dic_num_add[256],
			dic_add_idx;
		const uint8_t num_add = tNum * 8 - 7;

		//init
		const uint64_t mask = 0x1Ui64 << tID;
		unique_lock <mutex> lck(mtx_FindThread_Wait);
		memset(dic_num_add, 1, 256);
		for (auto a = 7; a < 256; a += 8)
		{
			dic_num_add[a] = num_add;
		}
		p_prefetch = (char*)dic_num_add;
		_mm_prefetch((char*)p_prefetch, _MM_HINT_T0);
#if DEBUG_Thr
		wchar_t msg[6][24];

		//prepare msg
		swprintf(msg[0], L"Dic_FThr %2d creat\n", tID);
		swprintf(msg[1], L"Dic_FThr %2d init\n", tID);
		swprintf(msg[2], L"Dic_FThr %2d lock\n", tID);
		swprintf(msg[3], L"Dic_FThr %2d unlock\n", tID);
		swprintf(msg[4], L"DFT %2d noti DC0\n", tID);
		swprintf(msg[5], L"DFT %2d wa<- DC0\n", tID);

		db_log(msg[0]);
#endif
		a_FT_state -= mask;
#if DEBUG_Thr
		db_log(msg[4]);
#endif
		cv_CtrlThread_Wait.notify_all();
			
		cv_FindThread_Wait.wait(lck, [=] {return a_FT_state & mask; });
#if DEBUG_Thr
		db_log(msg[5]);
#endif
		lck.unlock();
#if DEBUG_Thr
		db_log(msg[3]);
		db_log(msg[1]);
#endif
		//prefetch chkdata
		_mm_prefetch((char*)chkdata.data, _MM_HINT_T0);//data
		_mm_prefetch((char*)chkdata.data + 64, _MM_HINT_T0);//propety

		//main part
		auto func_findnext = [&]
		{
			//for (dic_num_next += dic_num_add[dic_add_idx++]; dic_num_next < DictSize_Cur; dic_num_next += dic_num_add[dic_add_idx++])
			//for (; (dic_num_next += dic_num_add[dic_add_idx++]) < DictSize_Cur;)
			//for (; (dic_num_next += (++dic_add_idx & 0x3?1:3)) < DictSize_Cur;)//slow
			for (; (dic_num_next += dic_num_add[(dic_add_idx++) & 0xf]) < DictSize_Cur;)
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
				if ((DictList[dic_num_next].tab >> chk_minval) & 0x1)
				//judge if len satisfy
					if ((maxpos_next = DictList[dic_num_next].len - chkdata.curlen) >= 0)
						break;//get it
			}
			if (dic_num_next >= DictSize_Cur)
				dic_num_next = 0xffff;//end it
			else
			{
			#if DEBUG_BUF_CHK
				if (!tID)
					db_log(2, L"get");
			#endif
				//prefetch next DictItem
				p_prefetch = (char *)&Diction[DictList[dic_num_next].dnum];//pos of next object DictItem
				_mm_prefetch(p_prefetch, _MM_HINT_T0);//-data
				//_mm_prefetch(p_prefetch + 64, _MM_HINT_T0);//-jump
				p_prefetch = (char *)&DictIdx[DictList[dic_num_next].dnum];//pos of the object DictIndex
				_mm_prefetch(p_prefetch + (chk_minval & 0xc0), _MM_HINT_NTA);//-index
				//prefetch next block
				p_prefetch = ((char*)&DictList[(dic_num_next + num_add) & 0xfffc]);
				_mm_prefetch(p_prefetch, _MM_HINT_T0);//next block info
				_mm_prefetch(p_prefetch + 64, _MM_HINT_T0);//next block info
			}
		};
		auto func_tonext = [&]
		{
			dicinfo = &DictList[dic_num_cur = dic_num_next];
			dicdata = &Diction[dicinfo->dnum];
			dicidx = &DictIdx[dicinfo->dnum];
			maxpos = maxpos_next;
			if (dicinfo->len > 32)
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
			//dic_dat = dicdata->data;
			//dic_jmp = dicdata->jump;
		};

		while (true)//run one cycle at a FindInDict
		{
			//refresh chker
			memcpy(&chkdata, inchk, sizeof(ChkItem));
			dic_num_next = dic_num_cur = tID * 8;
			dic_add_idx = 0;

			//locate dict
			/*dicinfo = &DictList[dic_num_cur];
			dicdata = &Diction[dicinfo->dnum];
			dicidx = &DictIdx[dicinfo->dnum];*/
			func_tonext();

			//prefetch current
			p_prefetch = (char *)dicdata;//pos of the object DictItem
			_mm_prefetch(p_prefetch, _MM_HINT_T0);//-data
			_mm_prefetch(p_prefetch + 64, _MM_HINT_T0);//-jump
			p_prefetch = (char *)dicidx;//pos of the object DictIndex
			_mm_prefetch(p_prefetch + (chk_minval & 0xc0), _MM_HINT_NTA);//-index
			
			//judge cur dict
			maxpos = dicinfo->len - chkdata.curlen;
			func_findnext();

			while(dic_num_cur < DictSize_Cur)//loop when finish a DictItem,fail OR suc(add chk)
			{
				findpos = 0x7f;
				//maxpos = dicinfo->len - chkdata.curlen;//max find pos(start) in the dict
				//maxpos:changed ahead 
				//objpos:must on-change
				objpos = dicidx->index[chk_minval];//object-bit pos in the dict
				if (objpos == 0x7f)//no matching word
				{
					if (dic_num_next == 0xffff)//no next
						break;

					func_tonext();
					func_findnext();
					continue;
				}
				while (objpos < chk_minpos)
					objpos = dic_jmp[objpos];
				//maxpos must < 64
				//when objpos = 0x7f,dicspos > 64 so dicspos must > maxpos
				//when objpos != 0x7f(<64) need to judge
				if ((dicspos = objpos - chk_minpos) > maxpos)//no enough space to match word
				{
					if (dic_num_next == 0xffff)//no next
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
					dicrep.dicID = dic_num_cur;
					dicrep.diclen = dicinfo->len;
					dicrep.offset = findpos;
					dicrep.objlen = chkdata.curlen;
					if (Chk_inc(chkdata) == 0)
						break;//chkdata is full used
					//add chk suc
					if (--maxpos_next < 0)//next fail
						func_findnext();
					if (--maxpos < 0)
					{//current failed
						if (dic_num_next == 0xffff)//no next
							break;
						
						func_tonext();
						func_findnext();
						continue;
					}
				}
				else//not find it
				{
					if (dic_num_next == 0xffff)//no next
						break;
					
					func_tonext();
					func_findnext();
					continue;
				}
			}
			//end of seaching the whole diction

			//send back signal to the control thread
			a_FT_state -= mask;
			lck.lock();
#if DEBUG_Thr
			db_log(msg[2]);
#endif
			if (dicrep.isFind && FindLen < dicrep.objlen)
				FindLen = dicrep.objlen;
#if DEBUG_Thr
			db_log(msg[4]);
#endif
			cv_CtrlThread_Wait.notify_all();
			cv_FindThread_Wait.wait(lck, [=] {return a_FT_state & mask; });
#if DEBUG_Thr
			db_log(msg[5]);
#endif
			//waked up from ctrl thread
			lck.unlock();
#if DEBUG_Thr
			db_log(msg[3]);
#endif
			if(op == 0xff)
				break;//break to stop the thread
		}
		a_FT_state -= mask;
		cv_CtrlThread_Wait.notify_all();
#if DEBUG_Thr
		db_log(msg[4]);
#endif
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
		for (int8_t a = 0; a < tCount; a++)
			t_find[a] = thread(FindThread_x64, tCount, a, ref(ftop), &chkdata, ref(drep[a]));

		a_FT_state = 0xffffffffffffffffUi64;

		for (int8_t a = 0; a < tCount; a++)
			t_find[a].detach();

		//wait for init of findthread
		cv_CtrlThread_Wait.wait(lck_FindThread, [=] {return a_FT_state == mask; });
		//FindThread init finish
#if DEBUG
		wchar_t db_str[120];
#endif
#if DEBUG_Thr
		db_log(L"Init ok %d Dic_Fthr.\n");
#endif
		//notify upper thread
		op.op = 0;
		cv_Dict_Ready.notify_all();
#if DEBUG_Thr
		db_log(L"DC0 noti M**\n");
#endif
		//give up the mutex to wake up upper thread
		cv_Dict_Use.wait(lck_DictUse, [&] {return op.op != 0; });
#if DEBUG_Thr
		db_log(L"DC0 wa<- M**\n");
#endif
		//start into the main part
		while (true)
		{
			for (int8_t a = 0; a < tCount; a++)
				drep[a].isFind = 0;
			FindLen = 0;
			ftop = 0;
			a_FT_state = 0xffffffffffffffffUi64;
#if _NODICT
			op.op = 0x7f;
			op.findlen = 0;
#else
			//wake up all find-thread
			cv_FindThread_Wait.notify_all();
#if DEBUG_Thr
			db_log(L"DC0 noti DTa\n");
#endif
			cv_CtrlThread_Wait.wait(lck_FindThread, [=] {return a_FT_state == mask; });
#if DEBUG_Thr
			db_log(L"DC0 wa<- DTa\n");
#endif
			//waked up from find-thread

			op.findlen = FindLen;
			op.op = 0x7f;
#endif

			cv_Dict_Ready.notify_all();
#if DEBUG_Thr
			db_log(L"DC0 noti M**\n");
#endif
			cv_Dict_Use.wait(lck_DictUse, [&] {return op.op != 0x7f; });
#if DEBUG_Thr
			db_log(L"DC0 wa<- M**\n");
#endif
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
#if DEBUG_Thr
		db_log(L"DC0 noti DTa\n");
#endif
		cv_CtrlThread_Wait.wait(lck_FindThread, [=] {return a_FT_state == mask; });
#if DEBUG_Thr
		db_log(L"DC0 wa<- DTa\n");
#endif
		//all find-thread finish
		op.op = 0x7f;
		cv_Dict_Ready.notify_all();
#if DEBUG_Thr
		db_log(L"DC0 noti M**\n");
#endif
		lck_DictUse.unlock();
		return;
	}

	uint8_t getDictLen(uint16_t dID)
	{
		DictInfo &dinfo = DictList[dID];
		return dinfo.len;
	}


	void dumpdict()
	{
		wchar_t db_str[120];
		swprintf(db_str, L"Prt Dic count=%d\n", DictSize_Cur);
		db_log(4, db_str);
		for (auto a = 0; a < DictSize_Cur; ++a)
		{
			swprintf(db_str, L"@Dic%5d oID=%d len=%d ben=%d\n", a,DictList[a].oID,DictList[a].len,DictList[a].benefit);
			db_log(4, db_str);
		}
		db_dump(2, (uint8_t*)Diction, DictSize_Max*sizeof(DictItem));
	}

	

}



