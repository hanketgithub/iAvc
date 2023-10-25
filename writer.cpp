//
//  writer.cpp
//  iAvc
//
//  Created by Hank Lee on 2023/10/05.
//  Copyright (c) 2023 Hank Lee. All rights reserved.
//

/******************************
 * include
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <string>


#include "common.h"
#include "bits.h"


using namespace std;


static const uint8_t ZZ_SCAN[16]  =
{  
  0,  1,  4,  8,  5,  2,  3,  6,  9, 12, 13, 10,  7, 11, 14, 15
};

static const uint8_t ZZ_SCAN8[64] =
{  0,  1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11,  4,  5,
   12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28,
   35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
   58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63
};

static
void scaling_list(OutputBitstream_t &bitstream, int32_t *scalingListinput, int32_t *scalingList, int sizeOfScalingList, bool *UseDefaultScalingMatrix)
{
    int j, scanj;
    int delta_scale, lastScale, nextScale;

    lastScale = 8;
    nextScale = 8;

    for (j = 0; j < sizeOfScalingList; j++)
    {
        scanj = (sizeOfScalingList==16) ? ZZ_SCAN[j]:ZZ_SCAN8[j];

        if (nextScale!=0)
        {
            delta_scale = scalingListinput[scanj] - lastScale; // Calculate delta from the scalingList data from the input file

            if (delta_scale > 127)
            {
                delta_scale = delta_scale - 256;
            }
            else if (delta_scale < -128)
            {
                delta_scale = delta_scale + 256;
            }

            WRITE_SVLC(bitstream, delta_scale, "delta_scale");

            nextScale = scalingListinput[scanj];

            *UseDefaultScalingMatrix |= (scanj == 0 && nextScale == 0); // Check first matrix value for zero
        }

        scalingList[scanj] = (short) ((nextScale==0) ? lastScale:nextScale); // Update the actual scalingList matrix with the correct values
        lastScale = scalingList[scanj];
    }
}


// 7.3.2.1.1 Sequence parameter set data syntax
void GenerateSPS(OutputBitstream_t &bitstream, SPS_t &sps)
{
    WRITE_CODE(bitstream, sps.profile_idc, 8, "profile_idc");

    WRITE_FLAG(bitstream, sps.constrained_set0_flag, "constrained_set0_flag");
    WRITE_FLAG(bitstream, sps.constrained_set1_flag, "constrained_set1_flag");
    WRITE_FLAG(bitstream, sps.constrained_set2_flag, "constrained_set2_flag");
    WRITE_FLAG(bitstream, sps.constrained_set3_flag, "constrained_set3_flag");
    WRITE_FLAG(bitstream, sps.constrained_set4_flag, "constrained_set4_flag");
    WRITE_FLAG(bitstream, sps.constrained_set5_flag, "constrained_set5_flag");

    WRITE_CODE(bitstream, 0, 2, "reserved_zero_2bits");

    WRITE_CODE(bitstream, sps.level_idc, 8, "level_idc");

    WRITE_UVLC(bitstream, sps.seq_parameter_set_id, "seq_parameter_set_id");

    uint8_t profile_idc = sps.profile_idc;
    if (profile_idc == 100 || profile_idc == 110 || profile_idc == 122 || profile_idc == 244
     || profile_idc == 44  || profile_idc == 83  || profile_idc == 86  || profile_idc == 118 
     || profile_idc == 128 || profile_idc == 138 || profile_idc == 139 || profile_idc == 134 
     || profile_idc == 135)
    {
        WRITE_UVLC(bitstream, sps.chroma_format_idc, "chroma_format_idc");
        if (sps.chroma_format_idc == 3)
        {
            WRITE_FLAG(bitstream, sps.separate_colour_plane_flag, "separate_colour_plane_flag");
        }

        WRITE_UVLC(bitstream, sps.bit_depth_luma_minus8, "bit_depth_luma_minus8");
        WRITE_UVLC(bitstream, sps.bit_depth_chroma_minus8, "bit_depth_chroma_minus8");
        WRITE_FLAG(bitstream, sps.qpprime_y_zero_transform_bypass_flag, "qpprime_y_zero_transform_bypass_flag");
        WRITE_FLAG(bitstream, sps.seq_scaling_matrix_present_flag, "seq_scaling_matrix_present_flag");
        if (sps.seq_scaling_matrix_present_flag)
        {
            for (int i = 0; i < ( ( sps.chroma_format_idc != 3 ) ? 8 : 12 ); i++)
            {
                WRITE_FLAG(bitstream, sps.seq_scaling_list_present_flag[i], "seq_scaling_list_present_flag");               
                if (sps.seq_scaling_list_present_flag[i])
                {
                    if (i < 6)
                    {
                        int32_t ScalingList4x4[6][16];

                        scaling_list(bitstream, sps.ScalingList4x4[ i ], ScalingList4x4[ i ], 16, sps.UseDefaultScalingMatrix4x4Flag);
                    }
                    else
                    {
                        int32_t ScalingList8x8[6][64];
                        scaling_list(bitstream, sps.ScalingList8x8[ i-6 ], ScalingList8x8[ i ], 64, sps.UseDefaultScalingMatrix8x8Flag);
                    }
                }
            }
        }
    }

    WRITE_UVLC(bitstream, sps.log2_max_frame_num_minus4, "log2_max_frame_num_minus4");

    WRITE_UVLC(bitstream, sps.pic_order_cnt_type, "pic_order_cnt_type");
    if (sps.pic_order_cnt_type == 0)
    {
        WRITE_UVLC(bitstream, sps.log2_max_pic_order_cnt_lsb_minus4, "log2_max_pic_order_cnt_lsb_minus4");
    }
    else if (sps.pic_order_cnt_type == 1)
    {
        WRITE_FLAG(bitstream, sps.delta_pic_order_always_zero_flag, "delta_pic_order_always_zero_flag");
        WRITE_SVLC(bitstream, sps.offset_for_non_ref_pic, "offset_for_non_ref_pic");
        WRITE_SVLC(bitstream, sps.offset_for_top_to_bottom_field, "offset_for_top_to_bottom_field");
        WRITE_UVLC(bitstream, sps.num_ref_frames_in_pic_order_cnt_cycle, "num_ref_frames_in_pic_order_cnt_cycle");
        for (uint32_t i = 0; i < sps.num_ref_frames_in_pic_order_cnt_cycle; i++)
        {
            WRITE_SVLC(bitstream, sps.offset_for_ref_frame[i], "offset_for_ref_frame");
        }
    }
#if 0
    uint32_t pic_order_cnt_type;
    uint32_t log2_max_pic_order_cnt_lsb_minus4;
    bool delta_pic_order_always_zero_flag;
    int32_t offset_for_non_ref_pic;
    int32_t offset_for_top_to_bottom_field;
    uint32_t num_ref_frames_in_pic_order_cnt_cycle;

    uint32_t max_num_ref_frames;
    bool gaps_in_frame_num_value_allowed_flag;
    uint32_t pic_width_in_mbs_minus1;
    uint32_t pic_height_in_map_units_minus1;
    bool frame_mbs_only_flag;
    bool mb_adaptive_frame_field_flag = false;
    bool direct_8x8_inference_flag;
    bool frame_cropping_flag;
    uint32_t frame_crop_left_offset;
    uint32_t frame_crop_right_offset;
    uint32_t frame_crop_top_offset;
    uint32_t frame_crop_bottom_offset;

    bool vui_parameters_present_flag;


    pic_order_cnt_type = READ_UVLC(bitstream, "pic_order_cnt_type");
    if (pic_order_cnt_type == 0)
    {
        log2_max_pic_order_cnt_lsb_minus4 = READ_UVLC(bitstream, "log2_max_pic_order_cnt_lsb_minus4");
    }
    else if (pic_order_cnt_type == 1)
    {
        delta_pic_order_always_zero_flag    = READ_FLAG(bitstream, "delta_pic_order_always_zero_flag");
        offset_for_non_ref_pic              = READ_SVLC(bitstream, "offset_for_non_ref_pic");
        offset_for_top_to_bottom_field      = READ_SVLC(bitstream, "offset_for_top_to_bottom_field");
        num_ref_frames_in_pic_order_cnt_cycle = READ_UVLC(bitstream, "num_ref_frames_in_pic_order_cnt_cycle");
        for (uint32_t i = 0; i < sps.num_ref_frames_in_pic_order_cnt_cycle; i++)
        {
            sps.offset_for_ref_frame[ i ] = READ_SVLC(bitstream, "offset_for_ref_frame");
        }
    }

    max_num_ref_frames = READ_UVLC(bitstream, "max_num_ref_frames");
    gaps_in_frame_num_value_allowed_flag = READ_FLAG(bitstream, "gaps_in_frame_num_value_allowed_flag");
    pic_width_in_mbs_minus1 = READ_UVLC(bitstream, "pic_width_in_mbs_minus1");
    pic_height_in_map_units_minus1 = READ_UVLC(bitstream, "pic_height_in_map_units_minus1");
    frame_mbs_only_flag = READ_FLAG(bitstream, "frame_mbs_only_flag");
    if (!frame_mbs_only_flag)
    {
        mb_adaptive_frame_field_flag = READ_FLAG(bitstream, "mb_adaptive_frame_field_flag");
    }

    direct_8x8_inference_flag = READ_FLAG(bitstream, "direct_8x8_inference_flag");

    frame_cropping_flag = READ_FLAG(bitstream, "frame_cropping_flag");
    if (frame_cropping_flag)
    {
        frame_crop_left_offset      = READ_UVLC(bitstream, "frame_crop_left_offset");
        frame_crop_right_offset     = READ_UVLC(bitstream, "frame_crop_right_offset");
        frame_crop_top_offset       = READ_UVLC(bitstream, "frame_crop_top_offset");
        frame_crop_bottom_offset    = READ_UVLC(bitstream, "frame_crop_bottom_offset");
    }

    vui_parameters_present_flag = READ_FLAG(bitstream, "vui_parameters_present_flag");
    if (vui_parameters_present_flag)
    {
        vui_parameters(bitstream, sps.vui_seq_parameters);
    }


    while (bitstream.m_num_held_bits)
    {
        READ_CODE(bitstream, 1, "trailing_bit");
    }
#endif
}

