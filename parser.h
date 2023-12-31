

#ifndef ___I_AVC_PARSER_H___
#define ___I_AVC_PARSER_H___


extern void ParseSPS(InputBitstream_t &bitstream, SPS_t SPSs[], AvcInfo_t &pAvcInfo);

extern void ParsePPS(InputBitstream_t &bitstream, PPS_t PPSs[], SPS_t SPSs[]);

extern int ParseSlice
(
    InputBitstream_t &bitstream,
    Slice_t &slice,
    SPS_t SPSs[],
    PPS_t PPSs[],
    bool IdrPicFlag,
    uint8_t nal_ref_idc,
    std::string &message
);

#endif

