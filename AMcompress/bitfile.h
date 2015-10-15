#pragma once

namespace acp
{
	class bitRfile
	{
	private:
		FILE *file;
		uint8_t cdata[1024];
		uint16_t Buf_Pos_cur,//Current Buffer Position,
			Buf_Pos_max,//Max Buffer Position
			Buf_Size_max;//Max Buffer Size
		uint64_t FSize_left, FSize_tol;//FileSize Left,FileSize Total
		bool flush();
	public:
		bitRfile();
		~bitRfile();
		bool open(wstring &FileName);
		void close();
		bool getNext();
		uint32_t getBits(uint8_t num);
		uint16_t getChars(const uint16_t num, uint8_t *pOut);

		bool eof();
		uint64_t size();
		uint16_t getpos();
		uint64_t pos();
	};

	class bitWfile
	{
	private:
		FILE *file;
		uint8_t cdata[1024];
		uint16_t Buf_Pos_cur,//Current Buffer Position,
			Buf_Pos_max,//Max Buffer Position
			Buf_Size_max;//Max Buffer Size
		uint64_t FSize_tol;//FileSize that has been writen to the file
	public:
		bitWfile();
		~bitWfile();
		bool open(wstring &FileName);
		uint64_t close();
		bool flush(bool isAll = false);
		bool putNext(bool data);
		bool putBits(uint8_t num, uint32_t data);
		bool putChars(uint16_t num, const uint8_t *pInv);
		
		uint64_t size();
		uint16_t getpos();
		uint64_t pos();
		void show();
	};

}