//
//  parser.cpp
//  iAvc
//
//  Created by Hank Lee on 2023/10/05.
//  Copyright (c) 2023 Hank Lee. All rights reserved.
//

/******************************
 * include
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <string>


#include "common.h"
#include "bits.h"


using namespace std;



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


static void rbsp_trailing_bits(InputBitstream_t &bitstream)
{
    READ_FLAG(bitstream, "rbsp_stop_one_bit");
    while (bitstream.m_num_held_bits)
    {
        READ_FLAG(bitstream, "rbsp_alignment_zero_bit");
    }
}


static void scaling_list
(
    InputBitstream_t &bitstream,
    vector<int32_t> &scalingList,
    int sizeOfScalingList, 
    bool &useDefaultScalingMatrixFlag
) 
{
    int lastScale = 8;
    int nextScale = 8;

    for (int j = 0; j < sizeOfScalingList; j++) 
    {
        if (nextScale != 0)
        {
            int delta_scale = READ_SVLC(bitstream, "delta_scale");
            nextScale = ( lastScale + delta_scale + 256 ) % 256;
            useDefaultScalingMatrixFlag = ( j == 0 && nextScale == 0 );
        }
        scalingList[ j ] = ( nextScale == 0 ) ? lastScale : nextScale;
        lastScale = scalingList[ j ];
    }
}


// E.1.2 HRD parameters syntax
static void hrd_parameters
(
    InputBitstream_t &bitstream,
    HRD_t &hrd
)
{
    uint32_t cpb_cnt_minus1;
    uint32_t bit_rate_scale;
    uint32_t cpb_size_scale;
    uint32_t initial_cpb_removal_delay_length_minus1;
    uint32_t cpb_removal_delay_length_minus1;
    uint32_t dpb_output_delay_length_minus1;
    uint32_t time_offset_length;

    cpb_cnt_minus1 = READ_UVLC(bitstream, "cpb_cnt_minus1");
    bit_rate_scale = READ_CODE(bitstream, 4, "bit_rate_scale");
    cpb_size_scale = READ_CODE(bitstream, 4, "cpb_size_scale");
    for (int SchedSelIdx = 0; SchedSelIdx <= cpb_cnt_minus1; SchedSelIdx++)
    {
        hrd.bit_rate_value_minus1[SchedSelIdx] = READ_UVLC(bitstream, "bit_rate_value_minus1");
        hrd.cpb_size_value_minus1[SchedSelIdx] = READ_UVLC(bitstream, "cpb_size_value_minus1");
        hrd.cbr_flag[SchedSelIdx] = READ_FLAG(bitstream, "cbr_flag");
    }
    initial_cpb_removal_delay_length_minus1 = READ_CODE(bitstream, 5, "initial_cpb_removal_delay_length_minus1");
    cpb_removal_delay_length_minus1 = READ_CODE(bitstream, 5, "cpb_removal_delay_length_minus1");
    dpb_output_delay_length_minus1 = READ_CODE(bitstream, 5, "dpb_output_delay_length_minus1");
    time_offset_length = READ_CODE(bitstream, 5, "time_offset_length");

    hrd.cpb_cnt_minus1 = cpb_cnt_minus1;
    hrd.bit_rate_scale = bit_rate_scale;
    hrd.cpb_size_scale = cpb_size_scale;
    hrd.initial_cpb_removal_delay_length_minus1 = initial_cpb_removal_delay_length_minus1;
    hrd.cpb_removal_delay_length_minus1 = cpb_removal_delay_length_minus1;
    hrd.dpb_output_delay_length_minus1 = dpb_output_delay_length_minus1;
    hrd.time_offset_length = time_offset_length;
}


// E.1.1 VUI parameters syntax
static void vui_parameters
(
    InputBitstream_t &bitstream,
    VUI_t &vui
)
{
    bool        aspect_ratio_info_present_flag          = false;    // u(1)
    uint8_t     aspect_ratio_idc                        = 0;        // u(8)
    uint16_t    sar_width                               = 0;        // u(16)
    uint16_t    sar_height                              = 0;        // u(16)
    bool        overscan_info_present_flag              = false;    // u(1)
    bool        overscan_appropriate_flag               = false;    // u(1)
    bool        video_signal_type_present_flag          = false;    // u(1)
    uint8_t     video_format                            = 0;        // u(3)
    bool        video_full_range_flag                   = false;    // u(1)
    bool        colour_description_present_flag         = false;    // u(1)
    uint8_t     colour_primaries                        = 0;        // u(8)
    uint8_t     transfer_characteristics                = 0;        // u(8)
    uint8_t     matrix_coefficients                     = 0;        // u(8)
    bool        chroma_location_info_present_flag       = false;    // u(1)
    uint32_t    chroma_sample_loc_type_top_field        = 0;        // ue(v)
    uint32_t    chroma_sample_loc_type_bottom_field     = 0;        // ue(v)
    bool        timing_info_present_flag                = false;    // u(1)
    uint32_t    num_units_in_tick                       = 0;        // u(32)
    uint32_t    time_scale                              = 0;        // u(32)
    bool        fixed_frame_rate_flag                   = false;    // u(1)
    bool        nal_hrd_parameters_present_flag         = false;    // u(1)
    bool        vcl_hrd_parameters_present_flag         = false;    // u(1)
    bool        low_delay_hrd_flag                      = false;    // u(1)
    bool        pic_struct_present_flag                 = false;    // u(1)
    bool        bitstream_restriction_flag              = false;    // u(1)
    bool        motion_vectors_over_pic_boundaries_flag = false;    // u(1)
    uint32_t    max_bytes_per_pic_denom                 = 0;        // ue(v)
    uint32_t    max_bits_per_mb_denom                   = 0;        // ue(v)
    uint32_t    log2_max_mv_length_vertical             = 0;        // ue(v)
    uint32_t    log2_max_mv_length_horizontal           = 0;        // ue(v)
    uint32_t    max_num_reorder_frames                  = 0;        // ue(v)
    uint32_t    max_dec_frame_buffering                 = 0;        // ue(v)


    aspect_ratio_info_present_flag = READ_FLAG(bitstream, "aspect_ratio_info_present_flag");
    if (aspect_ratio_info_present_flag)
    {
        aspect_ratio_idc = READ_CODE(bitstream, 8, "aspect_ratio_idc");
        if (aspect_ratio_idc == ASPECT_RATIO_EXTENDED_SAR)
        {
            sar_width = READ_CODE(bitstream, 16, "sar_width");
            sar_height = READ_CODE(bitstream, 16, "sar_height");
        }
    }

    overscan_info_present_flag = READ_FLAG(bitstream, "overscan_info_present_flag");
    if (overscan_info_present_flag)
    {
        overscan_appropriate_flag = READ_FLAG(bitstream, "overscan_appropriate_flag");
    }

    video_signal_type_present_flag = READ_FLAG(bitstream, "video_signal_type_present_flag");
    if (video_signal_type_present_flag)
    {
        video_format = READ_CODE(bitstream, 3, "video_format");
        video_full_range_flag = READ_FLAG(bitstream, "video_full_range_flag");
        colour_description_present_flag = READ_FLAG(bitstream, "colour_description_present_flag");
        if (colour_description_present_flag)
        {
            colour_primaries = READ_CODE(bitstream, 8, "colour_primaries");
            transfer_characteristics = READ_CODE(bitstream, 8, "transfer_characteristics");
            matrix_coefficients = READ_CODE(bitstream, 8, "matrix_coefficients");
        }
    }

    chroma_location_info_present_flag = READ_FLAG(bitstream, "chroma_location_info_present_flag");
    if (chroma_location_info_present_flag)
    {
        chroma_sample_loc_type_top_field = READ_UVLC(bitstream, "chroma_sample_loc_type_top_field");
        chroma_sample_loc_type_bottom_field = READ_UVLC(bitstream, "chroma_sample_loc_type_bottom_field");
    }

    timing_info_present_flag = READ_FLAG(bitstream, "timing_info_present_flag");
    if (timing_info_present_flag)
    {
        num_units_in_tick = READ_CODE(bitstream, 32, "num_units_in_tick");
        time_scale = READ_CODE(bitstream, 32, "time_scale");
        fixed_frame_rate_flag = READ_FLAG(bitstream, "fixed_frame_rate_flag");
    }

    nal_hrd_parameters_present_flag = READ_FLAG(bitstream, "nal_hrd_parameters_present_flag");
    if (nal_hrd_parameters_present_flag)
    {
        hrd_parameters(bitstream, vui.nal_hrd_parameters);
    }

    vcl_hrd_parameters_present_flag = READ_FLAG(bitstream, "vcl_hrd_parameters_present_flag");
    if (vcl_hrd_parameters_present_flag)
    {
        hrd_parameters(bitstream, vui.vcl_hrd_parameters);
    }

    if (nal_hrd_parameters_present_flag || vcl_hrd_parameters_present_flag)
    {
        low_delay_hrd_flag = READ_FLAG(bitstream, "low_delay_hrd_flag");
    }

    pic_struct_present_flag = READ_FLAG(bitstream, "pic_struct_present_flag");
    bitstream_restriction_flag = READ_FLAG(bitstream, "bitstream_restriction_flag");

    if (bitstream_restriction_flag)
    {
        motion_vectors_over_pic_boundaries_flag = READ_FLAG(bitstream, "motion_vectors_over_pic_boundaries_flag");
        max_bytes_per_pic_denom                 = READ_UVLC(bitstream, "max_bytes_per_pic_denom");
        max_bits_per_mb_denom                   = READ_UVLC(bitstream, "max_bits_per_mb_denom");
        log2_max_mv_length_horizontal           = READ_UVLC(bitstream, "log2_max_mv_length_horizontal");
        log2_max_mv_length_vertical             = READ_UVLC(bitstream, "log2_max_mv_length_vertical");
        max_num_reorder_frames                  = READ_UVLC(bitstream, "max_num_reorder_frames");
        max_dec_frame_buffering                 = READ_UVLC(bitstream, "max_dec_frame_buffering");
    }
    

    vui.aspect_ratio_info_present_flag  = aspect_ratio_info_present_flag;
    vui.aspect_ratio_idc                = aspect_ratio_idc;
    vui.sar_width                       = sar_width;
    vui.sar_height                      = sar_height;

    vui.overscan_info_present_flag  = overscan_info_present_flag;
    vui.overscan_appropriate_flag   = overscan_appropriate_flag;

    vui.video_signal_type_present_flag  = video_signal_type_present_flag;
    vui.video_format                    = video_format;
    vui.video_full_range_flag           = video_full_range_flag;
    vui.colour_description_present_flag = colour_description_present_flag;
    vui.colour_primaries                = colour_primaries;
    vui.transfer_characteristics        = transfer_characteristics;
    vui.matrix_coefficients             = matrix_coefficients;

    vui.chroma_location_info_present_flag   = chroma_location_info_present_flag;
    vui.chroma_sample_loc_type_top_field    = chroma_sample_loc_type_top_field;
    vui.chroma_sample_loc_type_bottom_field = chroma_sample_loc_type_bottom_field;

    vui.timing_info_present_flag = timing_info_present_flag;
    vui.num_units_in_tick = num_units_in_tick;
    vui.time_scale = time_scale;
    vui.fixed_frame_rate_flag = fixed_frame_rate_flag;

    vui.nal_hrd_parameters_present_flag = nal_hrd_parameters_present_flag;
    vui.vcl_hrd_parameters_present_flag = vcl_hrd_parameters_present_flag;

    vui.pic_struct_present_flag = pic_struct_present_flag;
    vui.bitstream_restriction_flag = bitstream_restriction_flag;
    vui.motion_vectors_over_pic_boundaries_flag = motion_vectors_over_pic_boundaries_flag;
    vui.max_bytes_per_pic_denom = max_bytes_per_pic_denom;
    vui.max_bits_per_mb_denom = max_bits_per_mb_denom;
    vui.log2_max_mv_length_horizontal = log2_max_mv_length_horizontal;
    vui.log2_max_mv_length_vertical = log2_max_mv_length_vertical;
    vui.max_num_reorder_frames = max_num_reorder_frames;
    vui.max_dec_frame_buffering = max_dec_frame_buffering;
}


// 7.3.3.1 Reference picture list modification syntax
static void ref_pic_list_modification(InputBitstream_t &bitstream, SliceType slice_type)
{
    bool ref_pic_list_modification_flag_l0 = false;
    bool ref_pic_list_modification_flag_l1 = false;
    uint32_t modification_of_pic_nums_idc;
    uint32_t abs_diff_pic_num_minus1;
    uint32_t long_term_pic_num;

    if (slice_type % 5 != 2 && slice_type % 5 != 4)
    {
        ref_pic_list_modification_flag_l0 = READ_FLAG(bitstream, "ref_pic_list_modification_flag_l0");
        if (ref_pic_list_modification_flag_l0)
        {
            do
            {
                modification_of_pic_nums_idc = READ_UVLC(bitstream, "modification_of_pic_nums_idc");
                if (modification_of_pic_nums_idc == 0 || modification_of_pic_nums_idc == 1)
                {
                    abs_diff_pic_num_minus1 = READ_UVLC(bitstream, "abs_diff_pic_num_minus1");
                }
                else if (modification_of_pic_nums_idc == 2)
                {
                    long_term_pic_num = READ_UVLC(bitstream, "long_term_pic_num");
                }
            } while (modification_of_pic_nums_idc != 3);
        }
    }

    if (slice_type % 5 == 1)
    {
        ref_pic_list_modification_flag_l1 = READ_FLAG(bitstream, "ref_pic_list_modification_flag_l1");
        if (ref_pic_list_modification_flag_l1)
        {
            do
            {
                modification_of_pic_nums_idc = READ_UVLC(bitstream, "modification_of_pic_nums_idc");
                if (modification_of_pic_nums_idc == 0 || modification_of_pic_nums_idc == 1)
                {
                    abs_diff_pic_num_minus1 = READ_UVLC(bitstream, "abs_diff_pic_num_minus1");
                }
                else if (modification_of_pic_nums_idc == 2)
                {
                    long_term_pic_num = READ_UVLC(bitstream, "long_term_pic_num");
                }
            } while (modification_of_pic_nums_idc != 3);
        }
    }
}


static void pred_weight_table(InputBitstream_t &bitstream, Slice_t &slice, SPS_t &sps, SliceType slice_type)
{
    slice.luma_log2_weight_denom = READ_UVLC(bitstream, "luma_log2_weight_denom");

    if (sps.chroma_format_idc != 0)
    {
        slice.chroma_log2_weight_denom = READ_UVLC(bitstream, "chroma_log2_weight_denom");
    }

    for (uint32_t i = 0; i <= slice.num_ref_idx_l0_active_minus1; i++)
    {
        slice.luma_weight_l0_flag = READ_FLAG(bitstream, "luma_weight_l0_flag");
        if (slice.luma_weight_l0_flag)
        {
            slice.luma_weight_l0[LIST_0][i] = READ_SVLC(bitstream, "luma_weight_l0");
            slice.luma_offset_l0[LIST_0][i] = READ_SVLC(bitstream, "luma_offset_l0");
        }

        if (sps.chroma_format_idc != 0)
        {
            slice.chroma_weight_l0_flag = READ_FLAG(bitstream, "chroma_weight_l0_flag");
            if (slice.chroma_weight_l0_flag)
            {
                for (int j = 0; j < 2; j++)
                {
                    slice.chroma_weight_l0[LIST_0][i][j] = READ_SVLC(bitstream, "chroma_weight_l0");
                    slice.chroma_offset_l0[LIST_0][i][j] = READ_SVLC(bitstream, "chroma_offset_l0");
                }
            }
        }
    }

    if (slice_type % 5 == 1)
    {
        for (uint32_t i = 0; i <= slice.num_ref_idx_l1_active_minus1; i++)
        {
            slice.luma_weight_l1_flag = READ_FLAG(bitstream, "luma_weight_l1_flag");
            if (slice.luma_weight_l1_flag)
            {
                slice.luma_weight_l1[LIST_0][i] = READ_SVLC(bitstream, "luma_weight_l1");
                slice.luma_offset_l1[LIST_0][i] = READ_SVLC(bitstream, "luma_offset_l1");
            }

            if (sps.chroma_format_idc != 0)
            {
                slice.chroma_weight_l1_flag = READ_FLAG(bitstream, "chroma_weight_l1_flag");
                if (slice.chroma_weight_l1_flag)
                {
                    for (int j = 0; j < 2; j++)
                    {
                        slice.chroma_weight_l1[LIST_0][i][j] = READ_SVLC(bitstream, "chroma_weight_l1");
                        slice.chroma_offset_l1[LIST_0][i][j] = READ_SVLC(bitstream, "chroma_offset_l1");
                    }
                }
            }
        }
    }
}


static void dec_ref_pic_marking(InputBitstream_t &bitstream, Slice_t &slice, bool IdrPicFlag)
{
    if (IdrPicFlag)
    {
        slice.no_output_of_prior_pics_flag = READ_FLAG(bitstream, "no_output_of_prior_pics_flag");
        slice.long_term_reference_flag = READ_FLAG(bitstream, "long_term_reference_flag");
    }
    else
    {
        slice.adaptive_ref_pic_marking_mode_flag = READ_FLAG(bitstream, "adaptive_ref_pic_marking_mode_flag");

        if (slice.adaptive_ref_pic_marking_mode_flag)
        {
            uint32_t memory_management_control_operation;
            do
            {
                memory_management_control_operation = READ_UVLC(bitstream, "memory_management_control_operation");
                if (memory_management_control_operation == 1 || memory_management_control_operation == 3)
                {
                    slice.difference_of_pic_nums_minus1 = READ_UVLC(bitstream, "");
                }
                if (memory_management_control_operation == 2)
                {
                    slice.long_term_pic_num = READ_UVLC(bitstream, "long_term_pic_num");
                }
                if (memory_management_control_operation == 3 || memory_management_control_operation == 6)
                {
                    slice.long_term_frame_idx = READ_UVLC(bitstream, "long_term_frame_idx");
                }
                if (memory_management_control_operation == 4)
                {
                    slice.max_long_term_frame_idx_plus1 = READ_UVLC(bitstream, "max_long_term_frame_idx_plus1");
                }
            } while (memory_management_control_operation != 0);
        }
    }
}


// 7.3.2.1.1 Sequence parameter set data syntax
void ParseSPS(InputBitstream_t &bitstream, SPS_t SPSs[], AvcInfo_t &pAvcInfo)
{
    uint8_t profile_idc;

    uint8_t constrained_set0_flag;
    uint8_t constrained_set1_flag;
    uint8_t constrained_set2_flag;
    uint8_t constrained_set3_flag;
    uint8_t constrained_set4_flag;
    uint8_t constrained_set5_flag;

    uint8_t level_idc;
    uint8_t seq_parameter_set_id;

    uint32_t chroma_format_idc;
    bool separate_colour_plane_flag = false;
    
    uint32_t bit_depth_luma_minus8;
    uint32_t bit_depth_chroma_minus8;
    bool qpprime_y_zero_transform_bypass_flag;

    bool seq_scaling_matrix_present_flag;
    bool seq_scaling_list_present_flag[12];
    vector< vector<int32_t> > ScalingList4x4(6, vector<int32_t>(16));
    vector< vector<int32_t> > ScalingList8x8(6, vector<int32_t>(64));
    bool UseDefaultScalingMatrix4x4Flag[6];
    bool UseDefaultScalingMatrix8x8Flag[6];

    uint32_t log2_max_frame_num_minus4;
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

    profile_idc = READ_CODE(bitstream, 8, "profile_idc");

    constrained_set0_flag = READ_FLAG(bitstream, "constrained_set0_flag");
    constrained_set1_flag = READ_FLAG(bitstream, "constrained_set1_flag");
    constrained_set2_flag = READ_FLAG(bitstream, "constrained_set2_flag");
    constrained_set3_flag = READ_FLAG(bitstream, "constrained_set3_flag");
    constrained_set4_flag = READ_FLAG(bitstream, "constrained_set4_flag");
    constrained_set5_flag = READ_FLAG(bitstream, "constrained_set5_flag");

    READ_CODE(bitstream, 2, "reserved_zero_2bits");

    level_idc = READ_CODE(bitstream, 8, "level_idc");
    seq_parameter_set_id = READ_UVLC(bitstream, "seq_parameter_set_id");

    // Set SPS ID valid
    SPS_t &sps = SPSs[seq_parameter_set_id];
    sps.isValid = true;

    if (profile_idc == 100 || profile_idc == 110 || profile_idc == 122 || profile_idc == 244
     || profile_idc == 44  || profile_idc == 83  || profile_idc == 86  || profile_idc == 118 
     || profile_idc == 128 || profile_idc == 138 || profile_idc == 139 || profile_idc == 134 
     || profile_idc == 135)
    {
        chroma_format_idc = READ_UVLC(bitstream, "chroma_format_idc");
        if (chroma_format_idc == 3)
        {
            separate_colour_plane_flag = READ_FLAG(bitstream, "separate_colour_plane_flag");
        }

        bit_depth_luma_minus8 = READ_UVLC(bitstream, "bit_depth_luma_minus8");
        bit_depth_chroma_minus8 = READ_UVLC(bitstream, "bit_depth_chroma_minus8");
        qpprime_y_zero_transform_bypass_flag = READ_FLAG(bitstream, "qpprime_y_zero_transform_bypass_flag");
        seq_scaling_matrix_present_flag = READ_FLAG(bitstream, "seq_scaling_matrix_present_flag");
        if (seq_scaling_matrix_present_flag)
        {
            for (int i = 0; i < ( ( chroma_format_idc != 3 ) ? 8 : 12 ); i++)
            {
                sps.seq_scaling_list_present_flag[i] = READ_FLAG(bitstream, "seq_scaling_list_present_flag");
                if (sps.seq_scaling_list_present_flag[i])
                {
                    if (i < 6)
                    {
                        scaling_list(bitstream, ScalingList4x4[ i ], 16, sps.UseDefaultScalingMatrix4x4Flag[ i ] );
                        for (int j = 0; i < ScalingList4x4[ i ].size(); j++)
                        {
                            sps.ScalingList4x4[i][j] = ScalingList4x4[ i ][ j ];
                        }
                    }
                    else
                    {
                        scaling_list(bitstream, ScalingList8x8[ i - 6 ], 64, sps.UseDefaultScalingMatrix8x8Flag[ i - 6 ] );
                        for (int j = 0; j < ScalingList8x8[ i - 6 ].size(); j++)
                        {
                            sps.ScalingList8x8[i-6][j] = ScalingList8x8[ i - 6 ][ j ];
                        }
                    }
                }
            }
        }
    }

    log2_max_frame_num_minus4 = READ_UVLC(bitstream, "log2_max_frame_num_minus4");
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

    rbsp_trailing_bits(bitstream);


    sps.profile_idc             = profile_idc;
    sps.constrained_set0_flag   = constrained_set0_flag;
    sps.constrained_set1_flag   = constrained_set1_flag;
    sps.constrained_set2_flag   = constrained_set2_flag;
    sps.constrained_set3_flag   = constrained_set3_flag;
    sps.constrained_set4_flag   = constrained_set4_flag;
    sps.constrained_set5_flag   = constrained_set5_flag;
    sps.level_idc               = level_idc;
    sps.seq_parameter_set_id    = seq_parameter_set_id;

    sps.chroma_format_idc = chroma_format_idc;
    sps.separate_colour_plane_flag = separate_colour_plane_flag;
    sps.bit_depth_luma_minus8 = bit_depth_luma_minus8;
    sps.bit_depth_chroma_minus8 = bit_depth_chroma_minus8;
    sps.qpprime_y_zero_transform_bypass_flag = qpprime_y_zero_transform_bypass_flag;
    sps.seq_scaling_matrix_present_flag = seq_scaling_matrix_present_flag;

    sps.log2_max_frame_num_minus4 = log2_max_frame_num_minus4;
    sps.pic_order_cnt_type = pic_order_cnt_type;
    sps.log2_max_pic_order_cnt_lsb_minus4 = log2_max_pic_order_cnt_lsb_minus4;
    sps.delta_pic_order_always_zero_flag = delta_pic_order_always_zero_flag;
    sps.offset_for_non_ref_pic = offset_for_non_ref_pic;
    sps.offset_for_top_to_bottom_field = offset_for_top_to_bottom_field;
    sps.num_ref_frames_in_pic_order_cnt_cycle = num_ref_frames_in_pic_order_cnt_cycle;

    sps.max_num_ref_frames = max_num_ref_frames;
    sps.gaps_in_frame_num_value_allowed_flag = gaps_in_frame_num_value_allowed_flag;
    sps.pic_width_in_mbs_minus1         = pic_width_in_mbs_minus1;
    sps.pic_height_in_map_units_minus1  = pic_height_in_map_units_minus1;
    sps.frame_mbs_only_flag             = frame_mbs_only_flag;
    sps.mb_adaptive_frame_field_flag    = mb_adaptive_frame_field_flag;    
    sps.direct_8x8_inference_flag       = direct_8x8_inference_flag;

    sps.frame_cropping_flag         = frame_cropping_flag;
    sps.frame_crop_left_offset      = frame_crop_left_offset;
    sps.frame_crop_right_offset     = frame_crop_right_offset;
    sps.frame_crop_top_offset       = frame_crop_top_offset;
    sps.frame_crop_bottom_offset    = frame_crop_bottom_offset;
    sps.vui_parameters_present_flag = vui_parameters_present_flag;
}


// 7.3.2.2 Picture parameter set data syntax
void ParsePPS(InputBitstream_t &bitstream, PPS_t PPSs[], SPS_t SPSs[])
{
    uint32_t pic_parameter_set_id;                              // ue(v)
    uint32_t seq_parameter_set_id;                              // ue(v)

    bool    entropy_coding_mode_flag;                           // u(1)
    bool    transform_8x8_mode_flag;                            // u(1)

    bool    pic_scaling_matrix_present_flag;                    // u(1)
    bool    pic_scaling_list_present_flag[12];                  // u(1)

    int32_t calingList4x4[6][16];                               // se(v)
    int32_t ScalingList8x8[6][64];                              // se(v)

    bool    UseDefaultScalingMatrix4x4Flag[6];
    bool    UseDefaultScalingMatrix8x8Flag[6];

    bool    bottom_field_pic_order_in_frame_present_flag;       // u(1)

    uint32_t num_slice_groups_minus1;                           // ue(v)
    uint32_t slice_group_map_type;                              // ue(v)
    uint32_t run_length_minus1[MAXnum_slice_groups_minus1];     // ue(v)
    uint32_t top_left[MAXnum_slice_groups_minus1];              // ue(v)
    uint32_t bottom_right[MAXnum_slice_groups_minus1];          // ue(v)

    bool     slice_group_change_direction_flag;                 // u(1)
    uint32_t slice_group_change_rate_minus1;                    // ue(v)
    uint32_t pic_size_in_map_units_minus1;                      // ue(v)
    uint8_t *slice_group_id;                                    // complete MBAmap u(v)

    int32_t num_ref_idx_l0_default_active_minus1;               // ue(v)
    int32_t num_ref_idx_l1_default_active_minus1;               // ue(v)
    bool    weighted_pred_flag;                                 // u(1)
    uint32_t  weighted_bipred_idc;                              // u(2)
    int32_t       pic_init_qp_minus26;                          // se(v)
    int32_t       pic_init_qs_minus26;                          // se(v)
    int32_t       chroma_qp_index_offset;                       // se(v)

    int32_t       cb_qp_index_offset;                           // se(v)
    int32_t       cr_qp_index_offset;                           // se(v)
    int32_t       second_chroma_qp_index_offset;                // se(v)

    bool   deblocking_filter_control_present_flag;              // u(1)
    bool   constrained_intra_pred_flag;                         // u(1)
    bool   redundant_pic_cnt_present_flag;                      // u(1)
    bool   vui_pic_parameters_flag;                             // u(1)

    pic_parameter_set_id = READ_UVLC(bitstream, "pic_parameter_set_id");
    seq_parameter_set_id = READ_UVLC(bitstream, "seq_parameter_set_id");

    if (!SPSs[seq_parameter_set_id].isValid)
    {
        printf("SPS %d is not activated!\n", seq_parameter_set_id);
        return;
    }

    PPS_t &pps = PPSs[pic_parameter_set_id];
    pps.isValid = true;

    entropy_coding_mode_flag = READ_FLAG(bitstream, "entropy_coding_mode_flag");
    bottom_field_pic_order_in_frame_present_flag = READ_FLAG(bitstream, "bottom_field_pic_order_in_frame_present_flag");
    num_slice_groups_minus1 = READ_UVLC(bitstream, "num_slice_groups_minus1");
    if (num_slice_groups_minus1 > 0)
    {
        slice_group_map_type = READ_UVLC(bitstream, "slice_group_map_type");
        if (slice_group_map_type == 0)
        {
            for (int i = 0; i <= num_slice_groups_minus1; i++)
            {
                pps.run_length_minus1[i] = READ_UVLC(bitstream, "run_length_minus1");
            }
        }
        else if (slice_group_map_type == 2)
        {
            for (int i = 0; i <= num_slice_groups_minus1; i++)
            {
                pps.top_left[i]     = READ_UVLC(bitstream, "top_left");
                pps.bottom_right[i] = READ_UVLC(bitstream, "bottom_right");
            }
        }
        else if (slice_group_map_type == 3 || slice_group_map_type == 4 || slice_group_map_type == 5)
        {
            slice_group_change_direction_flag = READ_FLAG(bitstream, "slice_group_change_direction_flag");
            slice_group_change_rate_minus1 = READ_UVLC(bitstream, "slice_group_change_rate_minus1");
        }
        else if (slice_group_map_type == 6)
        {
            pic_size_in_map_units_minus1 = READ_UVLC(bitstream, "pic_size_in_map_units_minus1");

            int num_slice_groups = num_slice_groups_minus1 + 1;

            int NumberBitsPerSliceGroupId = 1;
            if (num_slice_groups > 4)
            {
                NumberBitsPerSliceGroupId = 3;
            }
            else if (num_slice_groups > 2)
            {
                NumberBitsPerSliceGroupId = 2;
            }

            for (int i = 0; i <= pic_size_in_map_units_minus1; i++)
            {
                pps.slice_group_id[i] = READ_CODE(bitstream, NumberBitsPerSliceGroupId, "slice_group_id");
            }
        }
    }

    num_ref_idx_l0_default_active_minus1    = READ_UVLC(bitstream, "num_ref_idx_l0_default_active_minus1");
    num_ref_idx_l1_default_active_minus1    = READ_UVLC(bitstream, "num_ref_idx_l1_default_active_minus1");
    weighted_pred_flag                      = READ_FLAG(bitstream, "weighted_pred_flag");
    weighted_bipred_idc                     = READ_CODE(bitstream, 2, "weighted_bipred_idc");
    pic_init_qp_minus26                     = READ_SVLC(bitstream, "pic_init_qp_minus26");
    pic_init_qs_minus26                     = READ_SVLC(bitstream, "pic_init_qs_minus26");
    chroma_qp_index_offset                  = READ_SVLC(bitstream, "chroma_qp_index_offset");
    deblocking_filter_control_present_flag  = READ_FLAG(bitstream, "deblocking_filter_control_present_flag");
    constrained_intra_pred_flag             = READ_FLAG(bitstream, "constrained_intra_pred_flag");
    redundant_pic_cnt_present_flag          = READ_FLAG(bitstream, "redundant_pic_cnt_present_flag");

    pps.pic_parameter_set_id = pic_parameter_set_id;
    pps.seq_parameter_set_id = seq_parameter_set_id;

    pps.entropy_coding_mode_flag                        = entropy_coding_mode_flag;
    pps.bottom_field_pic_order_in_frame_present_flag    = bottom_field_pic_order_in_frame_present_flag;
    pps.num_slice_groups_minus1                         = num_slice_groups_minus1;
    pps.slice_group_map_type                            = slice_group_map_type;
    pps.slice_group_change_direction_flag               = slice_group_change_direction_flag;
    pps.slice_group_change_rate_minus1                  = slice_group_change_rate_minus1;
    pps.pic_size_in_map_units_minus1                    = pic_size_in_map_units_minus1;

    pps.num_ref_idx_l0_default_active_minus1    = num_ref_idx_l0_default_active_minus1;
    pps.num_ref_idx_l1_default_active_minus1    = num_ref_idx_l1_default_active_minus1;
    pps.weighted_pred_flag                      = weighted_pred_flag;
    pps.weighted_bipred_idc                     = weighted_bipred_idc;
    pps.pic_init_qp_minus26                     = pic_init_qp_minus26;
    pps.pic_init_qs_minus26                     = pic_init_qs_minus26;
    pps.chroma_qp_index_offset                  = chroma_qp_index_offset;
    pps.deblocking_filter_control_present_flag  = deblocking_filter_control_present_flag;
    pps.constrained_intra_pred_flag             = constrained_intra_pred_flag;
    pps.redundant_pic_cnt_present_flag          = redundant_pic_cnt_present_flag;

    // if (MORE_RBSP_DATA())
    // ...
}


// 7.3.3 Slice header syntax
void ParseSliceHeader
(
    InputBitstream_t &bitstream,
    Slice_t &slice,
    SPS_t SPSs[],
    PPS_t PPSs[],
    bool IdrPicFlag,
    uint8_t nal_ref_idc,
    string &message
)
{
    uint32_t first_mb_in_slice;
    SliceType slice_type;
    uint32_t pic_parameter_set_id;
    uint8_t colour_plane_id;
    uint16_t frame_num;
    bool field_pic_flag;
    bool bottom_field_flag;
    uint32_t idr_pic_id;
    uint32_t pic_order_cnt_lsb;
    int32_t delta_pic_order_cnt[2];
    uint32_t redundant_pic_cnt;
    bool direct_spatial_mv_pred_flag = false;
    bool num_ref_idx_active_override_flag = false;
    uint32_t num_ref_idx_l0_active_minus1 = 0;
    uint32_t num_ref_idx_l1_active_minus1 = 0;
    uint32_t cabac_init_idc = 0;
    int32_t slice_qp_delta = 0;
    bool sp_for_switch_flag = false;
    int32_t slice_qs_delta = 0;
    uint32_t disable_deblocking_filter_idc = 0;
    int32_t slice_alpha_c0_offset_div2 = 0;
    int32_t slice_beta_offset_div2 = 0;
    uint32_t slice_group_change_cycle = 0;


    first_mb_in_slice       = READ_UVLC(bitstream, "first_mb_in_slice");
    slice_type              = (SliceType) READ_UVLC(bitstream, "slice_type");
    pic_parameter_set_id    = READ_UVLC(bitstream, "pic_parameter_set_id");

    if (!PPSs[pic_parameter_set_id].isValid)
    {
        printf("PPS %d is not activated!\n", pic_parameter_set_id);
        return;
    }

    PPS_t &pps = PPSs[ pic_parameter_set_id ];
    SPS_t &sps = SPSs[ pps.seq_parameter_set_id ];

    if (sps.separate_colour_plane_flag)
    {
        colour_plane_id = READ_CODE(bitstream, 2, "colour_plane_id");
    }

    frame_num = READ_CODE(bitstream, (sps.log2_max_frame_num_minus4 + 4), "frame_num");

    if (!sps.frame_mbs_only_flag)
    {
        field_pic_flag = READ_FLAG(bitstream, "field_pic_flag");

        if (field_pic_flag)
        {
            bottom_field_flag = READ_FLAG(bitstream, "bottom_field_flag");
        }
    }

    if (IdrPicFlag)
    {
        idr_pic_id = READ_UVLC(bitstream, "idr_pic_id");
    }

    if (sps.pic_order_cnt_type == 0)
    {
        pic_order_cnt_lsb = READ_CODE(bitstream, (sps.log2_max_pic_order_cnt_lsb_minus4 + 4), "pic_order_cnt_lsb");
        if (pps.bottom_field_pic_order_in_frame_present_flag && !field_pic_flag)
        {
            int32_t delta_pic_order_cnt_bottom = READ_SVLC(bitstream, "delta_pic_order_cnt_bottom");
        }
    }

    if (sps.pic_order_cnt_type == 1 && !sps.delta_pic_order_always_zero_flag)
    {
        delta_pic_order_cnt[0] = READ_SVLC(bitstream, "delta_pic_order_cnt[0]");

        if (pps.bottom_field_pic_order_in_frame_present_flag && !field_pic_flag)
        {
            delta_pic_order_cnt[1] = READ_SVLC(bitstream, "delta_pic_order_cnt[1]");
        }
    }

    if (pps.redundant_pic_cnt_present_flag)
    {
        redundant_pic_cnt = READ_UVLC(bitstream, "redundant_pic_cnt");
    }

    if (slice_type == B_SLICE)
    {
        direct_spatial_mv_pred_flag = READ_FLAG(bitstream, "direct_spatial_mv_pred_flag");
    }
    if (slice_type == P_SLICE || slice_type == SP_SLICE || slice_type == B_SLICE)
    {
        num_ref_idx_active_override_flag = READ_FLAG(bitstream, "num_ref_idx_active_override_flag");
        if (num_ref_idx_active_override_flag)
        {
            num_ref_idx_l0_active_minus1 = READ_UVLC(bitstream, "num_ref_idx_l0_active_minus1");
            if (slice_type == B_SLICE)
            {
                num_ref_idx_l1_active_minus1 = READ_UVLC(bitstream, "num_ref_idx_l1_active_minus1");
            }
        }
    }

    ref_pic_list_modification(bitstream, slice_type);

    if ( (pps.weighted_pred_flag && (slice_type == P_SLICE || slice_type == SP_SLICE))
      || (pps.weighted_bipred_idc == 1 && slice_type == B_SLICE) )
    {
        pred_weight_table(bitstream, slice, sps, slice_type);
    }

    if (nal_ref_idc != 0)
    {
        dec_ref_pic_marking(bitstream, slice, IdrPicFlag);
    }

    if (pps.entropy_coding_mode_flag && slice_type != I_SLICE && slice_type != SI_SLICE)
    {
        cabac_init_idc = READ_UVLC(bitstream, "cabac_init_idc");
    }

    slice_qp_delta = READ_SVLC(bitstream, "slice_qp_delta");

    if (slice_type == SP_SLICE || slice_type == SI_SLICE)
    {
        if (slice_type == SP_SLICE)
        {
            sp_for_switch_flag = READ_FLAG(bitstream, "sp_for_switch_flag");
        }

        slice_qs_delta = READ_SVLC(bitstream, "slice_qs_delta");
    }

    if (pps.deblocking_filter_control_present_flag)
    {
        disable_deblocking_filter_idc = READ_UVLC(bitstream, "disable_deblocking_filter_idc");
        if (disable_deblocking_filter_idc != 1)
        {
            slice_alpha_c0_offset_div2 = READ_SVLC(bitstream, "slice_alpha_c0_offset_div2");
            slice_beta_offset_div2 = READ_SVLC(bitstream, "slice_beta_offset_div2");
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

        slice_group_change_cycle = READ_CODE(bitstream, len, "slice_group_change_cycle");
    }

    slice.first_mb_in_slice     = first_mb_in_slice;
    slice.slice_type            = slice_type;
    slice.pic_parameter_set_id  = pic_parameter_set_id;
    slice.colour_plane_id       = colour_plane_id;
    slice.frame_num             = frame_num;

    slice.num_ref_idx_l0_active_minus1 = num_ref_idx_l0_active_minus1;
    slice.num_ref_idx_l1_active_minus1 = num_ref_idx_l1_active_minus1;
}

