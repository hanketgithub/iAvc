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





static uint32_t CeilLog2(uint32_t uiVal)
{
    unsigned uiTmp = uiVal-1;
    unsigned uiRet = 0;
    
    while (uiTmp != 0)
    {
        uiTmp >>= 1;
        uiRet++;
    }
    return uiRet;
}


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

        scalingList[scanj] = (int32_t) ((nextScale == 0) ? lastScale : nextScale); // Update the actual scalingList matrix with the correct values
        lastScale = scalingList[scanj];
    }
}


static
void write_rbsp_trailing_bits(OutputBitstream_t &bitstream)
{
    WRITE_FLAG(bitstream, 1, "rbsp_stop_one_bit");
    while (bitstream.m_num_held_bits)
    {
        WRITE_FLAG(bitstream, 0, "rbsp_alignment_zero_bit");           
    }
}


static int get_picture_type(SliceType slice_type)
{
    // set this value to zero for transmission without signaling
    // that the whole picture has the same slice type
    int same_slicetype_for_whole_frame = 5;

    switch (slice_type)
    {
        case I_SLICE:
        {
            return 2 + same_slicetype_for_whole_frame;
            break;
        }
        case P_SLICE:
        {
            return 0 + same_slicetype_for_whole_frame;
            break;
        }
        case B_SLICE:
        {
            return 1 + same_slicetype_for_whole_frame;
            break;
        }
        case SP_SLICE:
        {
            return 3 + same_slicetype_for_whole_frame;
            break;
        }
        case SI_SLICE:
        {
            return 4 + same_slicetype_for_whole_frame;
            break;
        }
        default:
        {
            fprintf(stderr, "Picture Type not supported!");
            break;
        }
    }

    return 0;
}


/*!
 ********************************************************************************************
 * \brief
 *    writes the ref_pic_list_reordering syntax
 ********************************************************************************************
*/
static void write_ref_pic_list_modification(OutputBitstream_t &obs, Slice_t &slice)
{
    SliceType slice_type = slice.slice_type;

    if (slice_type != I_SLICE && slice_type != SI_SLICE)
    {
        WRITE_FLAG(obs, slice.ref_pic_list_modification_flag_l0, "ref_pic_list_modification_flag_l0");
        if (slice.ref_pic_list_modification_flag_l0)
        {
            for (auto v : slice.ref_pic_list_modification_q0)
            {
                uint32_t modification_of_pic_nums_idc = v[0];
                WRITE_UVLC(obs, v[0], "modification_of_pic_nums_idc");

                if (modification_of_pic_nums_idc == 0 || modification_of_pic_nums_idc == 1)
                {
                    uint32_t abs_diff_pic_num_minus1 = v[1];
                    WRITE_UVLC(obs, abs_diff_pic_num_minus1, "abs_diff_pic_num_minus1");
                }
                else if (modification_of_pic_nums_idc == 2)
                {
                    uint32_t long_term_pic_num = v[1];
                    WRITE_UVLC(obs, long_term_pic_num, "long_term_pic_num");
                }
            }
        }
    }

    if (slice_type == B_SLICE)
    {
        WRITE_FLAG(obs, slice.ref_pic_list_modification_flag_l1, "ref_pic_list_modification_flag_l1");
        if (slice.ref_pic_list_modification_flag_l1)
        {
            for (auto v : slice.ref_pic_list_modification_q1)
            {
                uint32_t modification_of_pic_nums_idc = v[0];
                WRITE_UVLC(obs, v[0], "modification_of_pic_nums_idc");

                if (modification_of_pic_nums_idc == 0 || modification_of_pic_nums_idc == 1)
                {
                    uint32_t abs_diff_pic_num_minus1 = v[1];
                    WRITE_UVLC(obs, abs_diff_pic_num_minus1, "abs_diff_pic_num_minus1");
                }
                else if (modification_of_pic_nums_idc == 2)
                {
                    uint32_t long_term_pic_num = v[1];
                    WRITE_UVLC(obs, long_term_pic_num, "long_term_pic_num");
                }
            }
        }
    }
}



/*!
 ********************************************************************************************
 * \brief
 *    writes the pred_weight_table syntax
 ********************************************************************************************
*/
static void write_pred_weight_table(OutputBitstream_t &obs, Slice_t &slice, SPS_t &sps)
{
    WRITE_UVLC(obs, slice.luma_log2_weight_denom, "luma_log2_weight_denom");

    if (sps.chroma_format_idc != 0)
    {
        WRITE_UVLC(obs, slice.chroma_log2_weight_denom, "chroma_log2_weight_denom");
    }

    for (uint32_t i = 0; i <= slice.num_ref_idx_l0_active_minus1; i++)
    {
        WRITE_FLAG(obs, slice.luma_weight_l0_flag, "luma_weight_l0_flag");
        if (slice.luma_weight_l0_flag)
        {
            WRITE_SVLC(obs, slice.luma_weight_l0[i], "luma_weight_l0");
            WRITE_SVLC(obs, slice.luma_offset_l0[i], "luma_offset_l0");
        }

        if (sps.chroma_format_idc != 0)
        {
            WRITE_FLAG(obs, slice.chroma_weight_l0_flag, "chroma_weight_l0_flag");
            if (slice.chroma_weight_l0_flag)
            {
                for (int j = 0; j < 2; j++)
                {
                    WRITE_SVLC(obs, slice.chroma_weight_l0[i][j], "chroma_weight_l0");
                    WRITE_SVLC(obs, slice.chroma_offset_l0[i][j], "chroma_offset_l0");
                }
            }
        }
    }

    if (slice.slice_type == B_SLICE)
    {
        for (uint32_t i = 0; i <= slice.num_ref_idx_l1_active_minus1; i++)
        {
            WRITE_FLAG(obs, slice.luma_weight_l1_flag, "luma_weight_l1_flag");
            if (slice.luma_weight_l1_flag)
            {
                WRITE_SVLC(obs, slice.luma_weight_l1[i], "luma_weight_l1");
                WRITE_SVLC(obs, slice.luma_offset_l1[i], "luma_offset_l1");
            }

            if (sps.chroma_format_idc != 0)
            {
                WRITE_FLAG(obs, slice.chroma_weight_l0_flag, "chroma_weight_l1_flag");
                if (slice.chroma_weight_l0_flag)
                {
                    for (int j = 0; j < 2; j++)
                    {
                        WRITE_SVLC(obs, slice.chroma_weight_l0[i][j], "chroma_weight_l1");
                        WRITE_SVLC(obs, slice.chroma_offset_l0[i][j], "chroma_offset_l1");
                    }
                }
            }
        }
    }
}


static void write_dec_ref_pic_marking(OutputBitstream_t &obs, Slice_t &slice, bool IdrPicFlag)
{
    if (IdrPicFlag)
    {
        WRITE_FLAG(obs, slice.no_output_of_prior_pics_flag, "no_output_of_prior_pics_flag");
        WRITE_FLAG(obs, slice.long_term_reference_flag, "long_term_reference_flag");
    }
    else
    {
        WRITE_FLAG(obs, slice.adaptive_ref_pic_marking_mode_flag, "adaptive_ref_pic_marking_mode_flag");

        if (slice.adaptive_ref_pic_marking_mode_flag)
        {
            uint32_t memory_management_control_operation;
            uint32_t difference_of_pic_nums_minus1;
            uint32_t long_term_pic_num;
            uint32_t long_term_frame_idx;
            uint32_t max_long_term_frame_idx_plus1;

            for (auto op : slice.memory_management_control_ops)
            {
                uint32_t memory_management_control_operation = op[0];

                if (memory_management_control_operation == 1 || memory_management_control_operation == 3)
                {
                    WRITE_UVLC(obs, memory_management_control_operation, "memory_management_control_operation");

                    uint32_t difference_of_pic_nums_minus1 = op[1];
                    WRITE_UVLC(obs, difference_of_pic_nums_minus1, "difference_of_pic_nums_minus1");
                }
                if (memory_management_control_operation == 2)
                {
                    WRITE_UVLC(obs, memory_management_control_operation, "memory_management_control_operation");

                    uint32_t long_term_pic_num = op[1];
                    WRITE_UVLC(obs, long_term_pic_num, "long_term_pic_num");
                }
                if (memory_management_control_operation == 3 || memory_management_control_operation == 6)
                {
                    WRITE_UVLC(obs, memory_management_control_operation, "memory_management_control_operation");

                    uint32_t long_term_frame_idx = op[1];
                    WRITE_UVLC(obs, long_term_frame_idx, "long_term_frame_idx");
                }
                if (memory_management_control_operation == 4)
                {
                    WRITE_UVLC(obs, memory_management_control_operation, "memory_management_control_operation");

                    uint32_t max_long_term_frame_idx_plus1 = op[1];
                    WRITE_UVLC(obs, max_long_term_frame_idx_plus1, "max_long_term_frame_idx_plus1");
                }
            }
        }
    }
}

static
void WriteHRDParameters
(
    OutputBitstream_t &bitstream,
    HRD_t &hrd
)
{
    WRITE_UVLC(bitstream, hrd.cpb_cnt_minus1, "cpb_cnt_minus1");
    WRITE_CODE(bitstream, hrd.bit_rate_scale, 4, "bit_rate_scale");
    WRITE_CODE(bitstream, hrd.cpb_size_scale, 4, "cpb_size_scale");
    for (int SchedSelIdx = 0; SchedSelIdx <= hrd.cpb_cnt_minus1; SchedSelIdx++)
    {
        WRITE_UVLC(bitstream, hrd.bit_rate_value_minus1[SchedSelIdx], "bit_rate_value_minus1");
        WRITE_UVLC(bitstream, hrd.cpb_size_value_minus1[SchedSelIdx], "cpb_size_value_minus1");
        WRITE_FLAG(bitstream, hrd.cbr_flag[SchedSelIdx], "cbr_flag");
    }    

    WRITE_CODE(bitstream, hrd.initial_cpb_removal_delay_length_minus1, 5, "initial_cpb_removal_delay_length_minus1");
    WRITE_CODE(bitstream, hrd.cpb_removal_delay_length_minus1, 5, "cpb_removal_delay_length_minus1");
    WRITE_CODE(bitstream, hrd.dpb_output_delay_length_minus1, 5, "dpb_output_delay_length_minus1");
    WRITE_CODE(bitstream, hrd.time_offset_length, 5, "time_offset_length");
}


static
void GenerateVUI
(
    OutputBitstream_t &bitstream,
    VUI_t &vui
)
{
    WRITE_FLAG(bitstream, vui.aspect_ratio_info_present_flag, "aspect_ratio_info_present_flag");
    if (vui.aspect_ratio_info_present_flag)
    {
        WRITE_CODE(bitstream, vui.aspect_ratio_idc, 8, "aspect_ratio_idc");
        if (vui.aspect_ratio_idc == ASPECT_RATIO_EXTENDED_SAR)
        {
            WRITE_CODE(bitstream, vui.sar_width, 16, "sar_width");
            WRITE_CODE(bitstream, vui.sar_height, 16, "sar_height");            
        }
    }

    WRITE_FLAG(bitstream, vui.overscan_info_present_flag, "overscan_info_present_flag");
    if (vui.overscan_info_present_flag)
    {
        WRITE_FLAG(bitstream, vui.overscan_appropriate_flag, "overscan_appropriate_flag");
    }

    WRITE_FLAG(bitstream, vui.video_signal_type_present_flag, "video_signal_type_present_flag");
    if (vui.video_signal_type_present_flag)
    {
        WRITE_CODE(bitstream, vui.video_format, 3, "video_format");
        WRITE_FLAG(bitstream, vui.video_full_range_flag, "video_full_range_flag");
        WRITE_FLAG(bitstream, vui.colour_description_present_flag, "colour_description_present_flag");
        if (vui.colour_description_present_flag)
        {
            WRITE_CODE(bitstream, vui.colour_primaries, 8, "colour_primaries");
            WRITE_CODE(bitstream, vui.transfer_characteristics, 8, "transfer_characteristics");
            WRITE_CODE(bitstream, vui.matrix_coefficients, 8, "matrix_coefficients");
        }
    }

    WRITE_FLAG(bitstream, vui.chroma_location_info_present_flag, "chroma_location_info_present_flag");
    if (vui.chroma_location_info_present_flag)
    {
        WRITE_UVLC(bitstream, vui.chroma_sample_loc_type_top_field, "chroma_sample_loc_type_top_field");
        WRITE_UVLC(bitstream, vui.chroma_sample_loc_type_bottom_field, "chroma_sample_loc_type_bottom_field");
    }

    WRITE_FLAG(bitstream, vui.timing_info_present_flag, "timing_info_present_flag");
    if (vui.timing_info_present_flag)
    {
        WRITE_CODE(bitstream, vui.num_units_in_tick, 32, "num_units_in_tick");
        WRITE_CODE(bitstream, vui.time_scale, 32, "time_scale");
        WRITE_FLAG(bitstream, vui.fixed_frame_rate_flag, "fixed_frame_rate_flag");
    }

    WRITE_FLAG(bitstream, vui.nal_hrd_parameters_present_flag, "nal_hrd_parameters_present_flag");
    if (vui.nal_hrd_parameters_present_flag)
    {
        WriteHRDParameters(bitstream, vui.nal_hrd_parameters);
    }

    WRITE_FLAG(bitstream, vui.vcl_hrd_parameters_present_flag, "vcl_hrd_parameters_present_flag");
    if (vui.vcl_hrd_parameters_present_flag)
    {
        WriteHRDParameters(bitstream, vui.vcl_hrd_parameters);
    }

    if (vui.nal_hrd_parameters_present_flag || vui.vcl_hrd_parameters_present_flag)
    {
        WRITE_FLAG(bitstream, vui.low_delay_hrd_flag, "low_delay_hrd_flag");
    }    

    WRITE_FLAG(bitstream, vui.pic_struct_present_flag, "pic_struct_present_flag");
    WRITE_FLAG(bitstream, vui.bitstream_restriction_flag, "bitstream_restriction_flag");    
    if (vui.bitstream_restriction_flag)
    {
        WRITE_FLAG(bitstream, vui.motion_vectors_over_pic_boundaries_flag, "motion_vectors_over_pic_boundaries_flag");
        WRITE_UVLC(bitstream, vui.max_bytes_per_pic_denom, "max_bytes_per_pic_denom");
        WRITE_UVLC(bitstream, vui.max_bits_per_mb_denom, "max_bits_per_mb_denom");
        WRITE_UVLC(bitstream, vui.log2_max_mv_length_horizontal, "log2_max_mv_length_horizontal");
        WRITE_UVLC(bitstream, vui.log2_max_mv_length_vertical, "log2_max_mv_length_vertical");
        WRITE_UVLC(bitstream, vui.max_num_reorder_frames, "max_num_reorder_frames");
        WRITE_UVLC(bitstream, vui.max_dec_frame_buffering, "max_dec_frame_buffering");
    }
}


// 7.3.2.1.1 Sequence parameter set data syntax
void GenerateSPS
(
    OutputBitstream_t &bitstream,
    SPS_t &sps
)
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

    WRITE_UVLC(bitstream, sps.max_num_ref_frames, "max_num_ref_frames");
    WRITE_FLAG(bitstream, sps.gaps_in_frame_num_value_allowed_flag, "gaps_in_frame_num_value_allowed_flag");
    WRITE_UVLC(bitstream, sps.pic_width_in_mbs_minus1, "pic_width_in_mbs_minus1");
    WRITE_UVLC(bitstream, sps.pic_height_in_map_units_minus1, "pic_height_in_map_units_minus1");
    WRITE_FLAG(bitstream, sps.frame_mbs_only_flag, "frame_mbs_only_flag");
    if (!sps.frame_mbs_only_flag)
    {
        WRITE_FLAG(bitstream, sps.mb_adaptive_frame_field_flag, "mb_adaptive_frame_field_flag");
    }

    WRITE_FLAG(bitstream, sps.direct_8x8_inference_flag, "direct_8x8_inference_flag");
    WRITE_FLAG(bitstream, sps.frame_cropping_flag, "frame_cropping_flag");
    if (sps.frame_cropping_flag)
    {
        WRITE_UVLC(bitstream, sps.frame_crop_left_offset, "frame_crop_left_offset");
        WRITE_UVLC(bitstream, sps.frame_crop_right_offset, "frame_crop_right_offset");
        WRITE_UVLC(bitstream, sps.frame_crop_top_offset, "frame_crop_top_offset");
        WRITE_UVLC(bitstream, sps.frame_crop_bottom_offset, "frame_crop_bottom_offset");
    }

    WRITE_FLAG(bitstream, sps.vui_parameters_present_flag, "vui_parameters_present_flag");
    if (sps.vui_parameters_present_flag)
    {
        GenerateVUI(bitstream, sps.vui_seq_parameters);
    }

    // flush out leftover bits
    write_rbsp_trailing_bits(bitstream);
}


static void GenerateSliceHeader
(
    OutputBitstream_t &obs,
    Slice_t &slice,
    SPS_t &sps,
    PPS_t &pps,
    bool IdrPicFlag,
    uint8_t nal_ref_idc
)
{
    WRITE_UVLC(obs, slice.first_mb_in_slice, "first_mb_in_slice");
    WRITE_UVLC(obs, get_picture_type(slice.slice_type), "slice_type");
    WRITE_UVLC(obs, slice.pic_parameter_set_id, "pic_parameter_set_id");

    SliceType slice_type = slice.slice_type;

    if (sps.separate_colour_plane_flag)
    {
        WRITE_CODE(obs, slice.colour_plane_id, 2, "colour_plane_id");
    }

    // write frame_num
    WRITE_CODE(obs, slice.frame_num, (sps.log2_max_frame_num_minus4 + 4), "frame_num");

    if (!sps.frame_mbs_only_flag)
    {
        WRITE_FLAG(obs, slice.field_pic_flag, "field_pic_flag");
        if (slice.field_pic_flag)
        {
            WRITE_FLAG(obs, slice.bottom_field_flag, "bottom_field_flag");
        }
    }

    if (IdrPicFlag)
    {
        WRITE_UVLC(obs, slice.idr_pic_id, "idr_pic_id");
    }

    if (sps.pic_order_cnt_type == 0)
    {
        WRITE_CODE(obs, slice.pic_order_cnt_lsb, (sps.log2_max_pic_order_cnt_lsb_minus4 + 4), "pic_order_cnt_lsb");
        if (pps.bottom_field_pic_order_in_frame_present_flag && !slice.field_pic_flag)
        {
            WRITE_SVLC(obs, slice.delta_pic_order_cnt_bottom, "delta_pic_order_cnt_bottom");
        }
    }

    if (sps.pic_order_cnt_type == 1 && !sps.delta_pic_order_always_zero_flag)
    {
        WRITE_SVLC(obs, slice.delta_pic_order_cnt[0], "delta_pic_order_cnt[0]");

        if (pps.bottom_field_pic_order_in_frame_present_flag && !slice.field_pic_flag)
        {
            WRITE_SVLC(obs, slice.delta_pic_order_cnt[1], "delta_pic_order_cnt[1]");
        }
    }

    if (pps.redundant_pic_cnt_present_flag)
    {
        WRITE_UVLC(obs, slice.redundant_pic_cnt, "redundant_pic_cnt");
    }

    if (slice_type == B_SLICE)
    {
        WRITE_FLAG(obs, slice.direct_spatial_mv_pred_flag, "direct_spatial_mv_pred_flag");
    }
    if (slice_type == P_SLICE || slice_type == SP_SLICE || slice_type == B_SLICE)
    {
        WRITE_FLAG(obs, slice.num_ref_idx_active_override_flag, "num_ref_idx_active_override_flag");
        if (slice.num_ref_idx_active_override_flag)
        {
            WRITE_UVLC(obs, slice.num_ref_idx_l0_active_minus1, "num_ref_idx_l0_active_minus1");

            if (slice_type == B_SLICE)
            {
                WRITE_UVLC(obs, slice.num_ref_idx_l1_active_minus1, "num_ref_idx_l1_active_minus1");
            }
        }
    }

    write_ref_pic_list_modification(obs, slice);

    if ( (pps.weighted_pred_flag && (slice_type == P_SLICE || slice_type == SP_SLICE))
      || (pps.weighted_bipred_idc == 1 && slice_type == B_SLICE) )
    {
        write_pred_weight_table(obs, slice, sps);
    }

    if (nal_ref_idc != 0)
    {
        write_dec_ref_pic_marking(obs, slice, IdrPicFlag);
    }

    if (pps.entropy_coding_mode_flag && slice_type != I_SLICE && slice_type != SI_SLICE)
    {
        WRITE_UVLC(obs, slice.cabac_init_idc, "cabac_init_idc");
    }

    WRITE_SVLC(obs, slice.slice_qp_delta, "slice_qp_delta");

    if (slice_type == SP_SLICE || slice_type == SI_SLICE)
    {
        if (slice_type == SP_SLICE)
        {
            WRITE_FLAG(obs, slice.sp_for_switch_flag, "sp_for_switch_flag");
        }

        WRITE_SVLC(obs, slice.slice_qs_delta, "slice_qs_delta");
    }

    if (pps.deblocking_filter_control_present_flag)
    {
        WRITE_UVLC(obs, slice.disable_deblocking_filter_idc, "disable_deblocking_filter_idc");
        if (slice.disable_deblocking_filter_idc != 1)
        {
            WRITE_SVLC(obs, slice.slice_alpha_c0_offset_div2, "slice_alpha_c0_offset_div2");
            WRITE_SVLC(obs, slice.slice_beta_offset_div2, "slice_beta_offset_div2");
        }
    }

    if (pps.num_slice_groups_minus1 > 0 && pps.slice_group_map_type >=3 && pps.slice_group_map_type <= 5)
    {
        int len = (sps.pic_height_in_map_units_minus1 + 1 ) * (sps.pic_width_in_mbs_minus1 + 1) / (pps.slice_group_change_rate_minus1 + 1);

        if (((sps.pic_height_in_map_units_minus1 + 1) * (sps.pic_width_in_mbs_minus1 + 1)) % (pps.slice_group_change_rate_minus1 + 1))
        {
            len += 1;
        }

        len = CeilLog2(len + 1);

        WRITE_CODE(obs, slice.slice_group_change_cycle, len, "slice_group_change_cycle");
    }
}


static void GenerateSliceData(OutputBitstream_t &obs, PPS_t &pps)
{
    if (pps.entropy_coding_mode_flag)
    {
        while (obs.m_num_held_bits > 0)
        {
            WRITE_CODE(obs, 1, 1, "cabac_alignment_one_bit");
        }
    }
}


void GenerateSlice
(
    OutputBitstream_t &obs,
    Slice_t &slice,
    SPS_t &sps,
    PPS_t &pps,
    bool IdrPicFlag,
    uint8_t nal_ref_idc
)
{
    GenerateSliceHeader(obs, slice, sps, pps, IdrPicFlag, nal_ref_idc);

    printf("%s: held bits=%d\n", __FUNCTION__, obs.m_num_held_bits);

    GenerateSliceData(obs, pps);
}

