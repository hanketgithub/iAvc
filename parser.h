

#ifndef ___I_AVC_PARSER_H___
#define ___I_AVC_PARSER_H___


extern void ParseSPS(InputBitstream_t &bitstream, AvcInfo_t *pAvcInfo);

extern void ParsePPS(InputBitstream_t &bitstream);

extern void ParseSliceHeader(InputBitstream_t &bitstream, NaluType nal_type, std::string &message);

#endif

