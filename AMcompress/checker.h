#pragma once

namespace acp
{
	struct ChkItem
	{
		uint8_t data[64];
		uint8_t minval,//current bit value of min-appear
			minpos,//current pos of min-appear
			limit = 64,//max len of this chkitem
			curlen,//current len of this chkitem
			mincnt,//appear-count of min-appear
			minposD;//current pos of min-appear
		uint16_t minvalD;
		uint8_t minvalDD;//current bit value of min-appear
		uint8_t empty[7];
		uint8_t counts[256];
		//uint16_t counts[64];
	};

	uint8_t Chk_inc(ChkItem &chkdat);
	uint8_t Chk_upd(ChkItem &chkdat, FILE *inf, const uint8_t count);//remove data and read new data
	uint8_t Chk_pre(ChkItem &chkdat, uint8_t count);//prepare chker
}