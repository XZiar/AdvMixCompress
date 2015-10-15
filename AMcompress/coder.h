#pragma once

namespace acp
{
	struct DictReport;
	struct BufferReport;
	class bitRfile;
	class bitWfile;
	struct CodeAns
	{
		uint8_t type;
		uint8_t srclen = 0;//source data len
		uint8_t part_num;
		uint8_t part_len[6];
		uint32_t part_data[6];
		uint16_t dID;
		int16_t savelen = (int16_t)0x8000;//save data len(bit)
		uint8_t *addr;
		uint8_t *p_b;
		uint32_t s_b;
	};

	struct CoderOP
	{
		uint8_t op;
		uint8_t bdata;
		uint8_t enddata[3] = { 0x0,0x0,0x0 };
		CodeAns cdata;
	};

	struct DecoderOP
	{
		uint8_t op;
		uint16_t len;
		uint16_t dID;
		uint32_t offset;
		uint8_t data[300];
	};

	extern mutex mtx_Coder_Use;
	extern condition_variable cv_Coder_Use,
		cv_Coder_Ready;

	CodeAns Code_TestDict(int8_t num, DictReport drep[]);
	CodeAns Code_TestBuffer(int8_t num, BufferReport brep[]);

	void CoderT(bitWfile &out, CoderOP &op);
	void Coder(bitWfile &out, CoderOP &op);
	bool DeCoder(bitRfile &in, DecoderOP &op);
}