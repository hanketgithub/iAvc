

#ifndef ___I_AVC_WRITER_H___
#define ___I_AVC_WRITER_H___


extern void GenerateSPS(OutputBitstream_t &bitstream, SPS_t &sps);

extern void GenerateSliceHeader
(
    OutputBitstream_t &obs,
    Slice_t &slice,
    SPS_t &sps,
    PPS_t &pps,
    bool IdrPicFlag,
    uint8_t nal_ref_idc
);

#endif

