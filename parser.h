

#ifndef ___I_AVC_PARSER_H___
#define ___I_AVC_PARSER_H___


extern void ParseSPS(InputBitstream_t &bitstream, SPS_t &sps, AvcInfo_t &pAvcInfo);

extern void ParsePPS(InputBitstream_t &bitstream);

extern void ParseSliceHeader(InputBitstream_t &bitstream, SPS_t &sps, std::string &message);


#endif

