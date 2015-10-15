#include "rely.h"
#include "bitfile.h"

namespace acp
{
	static uint32_t mask_keep[25];
	static uint8_t mask_get[10];

	static inline void prepare()
	{
		for (uint32_t a = 24, c = 0xffffff; a; c = c >> 1)//keep data up to 24bit from right
			mask_keep[a--] = c;
		for (uint8_t a = 0, c = 0x80; a < 8; c = c >> 1)//keep only one-bit data
			mask_get[a++] = c;
	}

	bitRfile::bitRfile()
	{
		Buf_Pos_max = 8192;
		Buf_Size_max = 1024;
		prepare();
	}

	bitRfile::~bitRfile()
	{
		close();
	}

	bool bitRfile::open(wstring &FileName)
	{
		file = _wfopen(FileName.c_str(), L"rb");
		if (file == NULL)
		{
			FSize_tol = 0;
			return false;
		}

		fseek(file, 0, SEEK_END);
		FSize_tol = ftell(file);
		fseek(file, 0, SEEK_SET);
		if (FSize_tol > Buf_Size_max)
		{
			//enough
			fread(cdata, 1, Buf_Size_max, file);
			FSize_left = FSize_tol - (Buf_Size_max);
		}
		else
		{
			//printf("small file\n");
			fread(cdata, 1, FSize_tol, file);
			FSize_left = 0;
			Buf_Size_max = (uint16_t)FSize_tol;
			Buf_Pos_max = Buf_Size_max * 8;
		}
		Buf_Pos_cur = 0;
		return true;
	}

	void bitRfile::close()
	{
		if (file)
			fclose(file);
	}

	bool bitRfile::flush()//need more data
	{
		bool ret = true;
		uint16_t len_move = (Buf_Pos_max - Buf_Pos_cur + 7) / 8;
		uint16_t Buf_Pos_Bcur = Buf_Pos_cur / 8;
		memmove(cdata, cdata + Buf_Pos_Bcur, len_move);
		if (FSize_left > Buf_Pos_Bcur)
		{//can fill the buffer
			FSize_left -= fread(cdata + len_move, 1, Buf_Pos_Bcur, file);
		}
		else if(FSize_left)
		{//still can read
			uint16_t a = (uint16_t)fread(cdata + len_move, 1, FSize_left, file);
			FSize_left -= a;
			Buf_Size_max = len_move + a;
			Buf_Pos_max = Buf_Size_max * 8;
		}
		else
		{
			//file end
			Buf_Size_max = len_move;
			Buf_Pos_max = Buf_Size_max * 8;
			ret = false;
		}
		Buf_Pos_cur = Buf_Pos_cur & 0x7;
		return ret;
	}

	bool bitRfile::getNext()
	{
		if (Buf_Pos_cur == Buf_Pos_max)
		{
			if (!flush())
				return false;
		}
		uint8_t ret = cdata[Buf_Pos_cur/8] & mask_get[Buf_Pos_cur & 0x7];
		++Buf_Pos_cur;
		return !!ret;
	}

	uint32_t bitRfile::getBits(uint8_t num)
	{
		if (num > 24 || num < 1)
			return 0x80000000;//input wrong
		if (Buf_Pos_cur + num > Buf_Pos_max)
		{
			if (!flush())
			{
				Buf_Pos_cur = Buf_Pos_max;
				return 0x80000001;//need too much
			}
		}
		uint16_t Buf_Pos_Bcur = Buf_Pos_cur / 8;
		uint8_t offset = 32 - (Buf_Pos_cur & 0x7) - num;
		uint32_t ret = ((uint32_t)cdata[Buf_Pos_Bcur] << 24) + ((uint32_t)cdata[Buf_Pos_Bcur + 1] << 16)
			+ ((uint32_t)cdata[Buf_Pos_Bcur + 2] << 8) + cdata[Buf_Pos_Bcur + 3];//4byte data
		ret = (ret >> offset) & mask_keep[num];//align at right and use mask to get bits
		Buf_Pos_cur += num;
		return ret;
	}

	uint16_t bitRfile::getChars(const uint16_t num, uint8_t* pOut)
	{
		if (num == 0 || num > Buf_Size_max)
			return 0x8000;//wrong input
		if (Buf_Pos_cur + num * 8 > Buf_Pos_max)
		{
			if (!flush())
			{
				Buf_Pos_cur = Buf_Pos_max;
				return 0x8001;//need too much
			}
		}
		uint8_t offseta = Buf_Pos_cur & 0x7;
		uint8_t offsetb = 8 - offseta;
		for (uint16_t Buf_Pos_Bcur = Buf_Pos_cur / 8, left = num; left--; ++Buf_Pos_Bcur)
			*pOut++ = (cdata[Buf_Pos_Bcur] << offseta) + (cdata[Buf_Pos_Bcur + 1] >> offsetb);
		
		Buf_Pos_cur += num * 8;
		return num;
	}

	bool bitRfile::eof()
	{
		return (FSize_left ? false : Buf_Pos_cur == Buf_Pos_max);
	}

	uint64_t bitRfile::size()
	{
		return FSize_tol;
	}

	uint16_t bitRfile::getpos()
	{
		return Buf_Pos_cur;
	}

	uint64_t bitRfile::pos()
	{
		return FSize_tol - FSize_left - (Buf_Pos_max - Buf_Pos_cur) / 8;
	}



	bitWfile::bitWfile()
	{
		Buf_Pos_max = 8192;
		Buf_Size_max = 1024;
		memset(cdata, 0, 1024);
		prepare();
	}

	bitWfile::~bitWfile()
	{
		flush(true);
		close();
	}

	bool bitWfile::open(wstring &FileName)
	{
		FSize_tol = 0;
		file = _wfopen(FileName.c_str(), L"wb");
		if (file == NULL)
		{
			return false;
		}
		Buf_Pos_cur = 0;
		return true;
	}

	uint64_t bitWfile::close()
	{
		if (file)
		{
			if (Buf_Pos_cur)
				flush(true);
			fclose(file);
			return FSize_tol;
		}
		else
			return UINT64_MAX;
	}

	bool bitWfile::flush(bool isAll)
	{
		uint16_t Buf_Pos_Bcur = Buf_Pos_cur / 8;
		fwrite(cdata, 1, Buf_Pos_Bcur, file);
		FSize_tol += Buf_Pos_Bcur;
		Buf_Pos_cur = Buf_Pos_cur & 0x7;
		if (isAll && Buf_Pos_cur)
		{
			fputc(cdata[Buf_Pos_Bcur], file);
			Buf_Pos_cur = 0;
			++FSize_tol;
			memset(cdata, 0, 1024);
		}
		else
		{
			cdata[0] = cdata[Buf_Pos_Bcur];
			memset(cdata + 1, 0, 1023);
		}
		return true;
	}

	bool bitWfile::putNext(bool data)
	{
		if (Buf_Pos_cur == Buf_Pos_max)
			flush();
		if (data)
		{
			uint16_t Buf_Pos_Bcur = Buf_Pos_cur / 8;
			cdata[Buf_Pos_Bcur] = cdata[Buf_Pos_Bcur] | mask_get[Buf_Pos_cur & 0x7];
		}
		++Buf_Pos_cur;
		return true;
	}

	bool bitWfile::putBits(uint8_t num, uint32_t data)
	{
		if (num > 23 || num < 1)
			return false;
		if (Buf_Pos_cur + num > Buf_Pos_max)
			flush();
		uint8_t mp = num + (Buf_Pos_cur & 0x7);
		uint16_t Buf_Pos_Bcur = Buf_Pos_cur >> 3;
		uint32_t ret = (uint32_t)cdata[Buf_Pos_Bcur] << 24;
		ret += (data & mask_keep[num]) << (32 - mp);
		mp = (mp + 7) / 8;
		for (uint8_t a = 24; mp--; a -= 8)
			cdata[Buf_Pos_Bcur++] = (ret >> a) & 0xff;
		Buf_Pos_cur += num;
		return true;
	}

	bool bitWfile::putChars(uint16_t num, const uint8_t *pIn)
	{
		uint8_t mp = Buf_Pos_cur & 0x7;
		if (Buf_Pos_cur + num * 8 > Buf_Pos_max)
			flush();
		uint8_t *pOut = cdata + Buf_Pos_cur / 8;
		Buf_Pos_cur += num * 8;
		if (!mp)//direct copy
			memcpy(pOut, pIn, num);
		else
		{
			uint8_t dat = *pOut;
			while (num--)
			{
				*pOut++ = dat + ((*pIn) >> mp);
				dat = (*pIn++) << (8 - mp);
			}
			*pOut = dat;
		}
		return true;
	}

	uint64_t bitWfile::size()
	{
		return FSize_tol;
	}

	uint16_t bitWfile::getpos()
	{
		return Buf_Pos_cur;
	}

	uint64_t bitWfile::pos()
	{
		return FSize_tol + Buf_Pos_cur / 8;
	}

	void bitWfile::show()
	{
		uint16_t max = (Buf_Pos_cur >> 3) + 1;
		printf("show current cache data with size=%d\n", Buf_Pos_cur);
		for (uint16_t a = 0; a < max; ++a)
		{
			printf("%2x%c", cdata[a], ((a + 1) & 0xf ? ',' : '\n'));
		}
		printf("\n");
		return;
	}

}