#include "rely.h"
#include "basic.h"
#include "checker.h"


namespace acp
{
	inline uint16_t hash(uint8_t * dat)
	{
		return (uint16_t)*(dat - 2) * 169 + (uint16_t)*(dat - 1) * 13 + *dat;
	}
	
	inline static void Chksort(ChkItem &chkdat, uint8_t objpos)
	{
		uint8_t curpos = objpos,
			curpos2,
			curcnt = 2;
		uint16_t objval = chkdat.list[objpos];
		for (; --curpos > 1;)
			//if (chkdat.list[curpos] == objval)
			if (!((chkdat.list[curpos] ^ objval) & 0x3ff))
				break;
		if (curpos == 1)//no repeat=>count = 1;
			return;
		//has repeat=>count > 1;
		for (curpos2 = curpos; --curpos2 > 1; ++curcnt)
			//if (chkdat.list[curpos2] != objval)
			if ((chkdat.list[curpos2] ^ objval) & 0x3ff)
				break;
		//curpos2+1 = left border,curpos = right border
		//move right data
		memmove(&chkdat.list[curpos + 2], &chkdat.list[curpos + 1], 2 * (objpos - curpos - 1));
		//move left data
		memmove(&chkdat.list[2 + curcnt], &chkdat.list[2], 2 * (curpos2 - 1));
		//put data
		for (auto a = 2; a < 2 + curcnt; ++a)
			chkdat.list[a] = objval;
		return;
	}
	
	uint8_t Chk_inc(ChkItem &chkdat)
	{
		if (chkdat.curlen == chkdat.limit)//cannot increase
			return 0;
		
		uint8_t dat = chkdat.data[chkdat.curlen];
		auto tmp = hash(&chkdat.data[chkdat.curlen]);
		chkdat.list[chkdat.curlen] = (tmp % 769) + ((uint16_t)chkdat.curlen << 10);
		Chksort(chkdat, chkdat.curlen);

		chkdat.minpos = chkdat.list[chkdat.curlen] >> 10;
		chkdat.minvalD = chkdat.list[chkdat.curlen] & 0x3ff;
		chkdat.minval = (chkdat.list[chkdat.curlen] & 0x3ff) % 53;
		
		return ++chkdat.curlen;//increase
	}

	uint8_t Chk_upd(ChkItem &chkdat, FILE *inf, const uint8_t count)
	{
		chkdat.curlen = 0;
		uint8_t left = chkdat.limit - count;
		if (left)//still data left
			memmove(chkdat.data, chkdat.data + count, left);
		uint8_t ret = (uint8_t)fread(chkdat.data + left, 1, count, inf);
		if (ret < count)
			chkdat.limit = left + ret;
		return chkdat.limit;
	}

	uint8_t Chk_pre(ChkItem &chkdat, uint8_t count)
	{
		if (chkdat.limit < count)
			return 0xff;
		chkdat.curlen = count;
		//memset(chkdat.counts, 0, 256);
		memset(chkdat.list, 0, sizeof(chkdat.list));

		for (uint8_t a = 2; a < count; ++a)
		{
			auto tmp = hash(&chkdat.data[a]);
			chkdat.list[a] = (tmp % 769) + ((uint16_t)a << 10);
			Chksort(chkdat, a);
		}
		chkdat.minpos = chkdat.list[--count] >> 10;
		chkdat.minvalD = chkdat.list[count] & 0x3ff;
		chkdat.minval = (chkdat.list[count] & 0x3ff) % 53;
		return 0;
		
	}
}