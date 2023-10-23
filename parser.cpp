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


static void scaling_list
(
    InputBitstream_t &bitstream,
    vector<int32_t> &scalingList,
    int sizeOfScalingList, 
    bool &useDefaultScalingMatrixFlag) 
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
    dpb_output_delay_length_minus1 = READ_CODE(bitstream , 5, "dpb_output_delay_length_minus1");
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
            sar_height = READ_CODE(bitstream , 16, "sar_height");
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

    vui.bitstream_restriction_flag = bitstream_restriction_flag;
    vui.motion_vectors_over_pic_boundaries_flag = motion_vectors_over_pic_boundaries_flag;
    vui.max_bytes_per_pic_denom = max_bytes_per_pic_denom;
    vui.max_bits_per_mb_denom = max_bits_per_mb_denom;
    vui.log2_max_mv_length_horizontal = log2_max_mv_length_horizontal;
    vui.log2_max_mv_length_vertical = log2_max_mv_length_vertical;
    vui.max_num_reorder_frames = max_num_reorder_frames;
    vui.max_dec_frame_buffering = max_dec_frame_buffering;
}


// 7.3.2.1.1 Sequence parameter set data syntax
void ParseSPS(InputBitstream_t &bitstream, SPS_t &sps, AvcInfo_t &pAvcInfo)
{
    uint8_t profile_idc;

    uint8_t constraint_set0_flag;
    uint8_t constraint_set1_flag;
    uint8_t constraint_set2_flag;
    uint8_t constraint_set3_flag;
    uint8_t constraint_set4_flag;
    uint8_t constraint_set5_flag;

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

    constraint_set0_flag = READ_FLAG(bitstream, "constraint_set0_flag");
    constraint_set1_flag = READ_FLAG(bitstream, "constraint_set1_flag");
    constraint_set2_flag = READ_FLAG(bitstream, "constraint_set2_flag");
    constraint_set3_flag = READ_FLAG(bitstream, "constraint_set3_flag");
    constraint_set4_flag = READ_FLAG(bitstream, "constraint_set4_flag");
    constraint_set5_flag = READ_FLAG(bitstream, "constraint_set5_flag");

    READ_CODE(bitstream, 2, "reserved_zero_2bits");

    level_idc = READ_CODE(bitstream, 8, "level_idc");
    seq_parameter_set_id = READ_UVLC(bitstream, "seq_parameter_set_id");

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


    sps.profile_idc             = profile_idc;
    sps.constrained_set0_flag   = constraint_set0_flag;
    sps.constrained_set1_flag   = constraint_set1_flag;
    sps.constrained_set2_flag   = constraint_set2_flag;
    sps.constrained_set3_flag   = constraint_set3_flag;
    sps.constrained_set4_flag   = constraint_set4_flag;
    sps.constrained_set5_flag   = constraint_set5_flag;
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


void ParsePPS(InputBitstream_t &bitstream, PPS_t &pps)
{
    pps.pic_parameter_set_id = READ_UVLC(bitstream, "pic_parameter_set_id");
    pps.seq_parameter_set_id = READ_UVLC(bitstream, "seq_parameter_set_id");
    pps.entropy_coding_mode_flag = READ_FLAG(bitstream, "entropy_coding_mode_flag");
    pps.bottom_field_pic_order_in_frame_present_flag = READ_FLAG(bitstream, "bottom_field_pic_order_in_frame_present_flag");
}


// 7.3.3 Slice header syntax
void ParseSliceHeader(InputBitstream_t &bitstream, SPS_t &sps, PPS_t &pps, bool IdrPicFlag, string &message)
{
    uint32_t first_mb_in_slice;
    uint32_t slice_type;
    uint32_t pic_parameter_set_id;
    uint8_t colour_plane_id;
    uint16_t frame_num;
    bool field_pic_flag;
    bool bottom_field_flag;
    uint32_t idr_pic_id;
    uint32_t pic_order_cnt_lsb;

    first_mb_in_slice       = READ_UVLC(bitstream, "first_mb_in_slice");
    slice_type              = READ_UVLC(bitstream, "slice_type");
    pic_parameter_set_id    = READ_UVLC(bitstream, "pic_parameter_set_id");

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
}

