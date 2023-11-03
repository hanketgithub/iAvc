//
//  common.h
//  iAvc
//
//  Created by Hank Lee on 2023/10/05
//  Copyright (c) 2023 hank. All rights reserved.
//

#ifndef ___I_AVC_COMMON_H___
#define ___I_AVC_COMMON_H___


#define BIT7            0x80
#define BIT6            0x40
#define BIT5            0x20
#define BIT4            0x10
#define BIT3            0x08
#define BIT2            0x04
#define BIT1            0x02
#define BIT0            0x01

//#define MAX_NUM_REF_PICS    16
#define MAXnum_ref_frames_in_pic_order_cnt_cycle    256
#define MAXnum_slice_groups_minus1                  8
#define MAXIMUMVALUEOFcpb_cnt                       32
#define MAX_REFERENCE_PICTURES                      32


typedef enum
{
    NALU_TYPE_SLICE    = 1,
    NALU_TYPE_DPA      = 2,
    NALU_TYPE_DPB      = 3,
    NALU_TYPE_DPC      = 4,
    NALU_TYPE_IDR      = 5,
    NALU_TYPE_SEI      = 6,
    NALU_TYPE_SPS      = 7,
    NALU_TYPE_PPS      = 8,
    NALU_TYPE_AUD      = 9,
    NALU_TYPE_EOSEQ    = 10,
    NALU_TYPE_EOSTREAM = 11,
    NALU_TYPE_FILL     = 12,
#if (MVC_EXTENSION_ENABLE)
    NALU_TYPE_PREFIX   = 14,
    NALU_TYPE_SUB_SPS  = 15,
    NALU_TYPE_SLC_EXT  = 20,
    NALU_TYPE_VDRD     = 24  // View and Dependency Representation Delimiter NAL Unit
#endif
} NaluType;


typedef enum
{
    ASPECT_RATIO_UNSPECIFIED    = 0,
    ASPECT_RATIO_1_1,
    ASPECT_RATIO_12_11,
    ASPECT_RATIO_10_11,
    ASPECT_RATIO_16_11,
    ASPECT_RATIO_40_33,
    ASPECT_RATIO_24_11,
    ASPECT_RATIO_20_11,
    ASPECT_RATIO_32_11,
    ASPECT_RATIO_80_33,
    ASPECT_RATIO_18_11,
    ASPECT_RATIO_15_11,
    ASPECT_RATIO_64_33,
    ASPECT_RATIO_160_99,
    ASPECT_RATIO_4_3,
    ASPECT_RATIO_3_2,
    ASPECT_RATIO_2_1,
    ASPECT_RATIO_EXTENDED_SAR   = 255,
} AspectRatioIdc;


typedef enum
{
    SEI_BUFFERING_PERIOD                     = 0,
    SEI_PICTURE_TIMING                       = 1,
    SEI_PAN_SCAN_RECT                        = 2,
    SEI_FILLER_PAYLOAD                       = 3,
    SEI_USER_DATA_REGISTERED_ITU_T_T35       = 4,
    SEI_USER_DATA_UNREGISTERED               = 5,
    SEI_RECOVERY_POINT                       = 6,
    SEI_SCENE_INFO                           = 9,
    SEI_FULL_FRAME_SNAPSHOT                  = 15,
    SEI_PROGRESSIVE_REFINEMENT_SEGMENT_START = 16,
    SEI_PROGRESSIVE_REFINEMENT_SEGMENT_END   = 17,
    SEI_FILM_GRAIN_CHARACTERISTICS           = 19,
    SEI_POST_FILTER_HINT                     = 22,
    SEI_TONE_MAPPING_INFO                    = 23,
    SEI_FRAME_PACKING                        = 45,
    SEI_DISPLAY_ORIENTATION                  = 47,
    SEI_SOP_DESCRIPTION                      = 128,
    SEI_ACTIVE_PARAMETER_SETS                = 129,
    SEI_DECODING_UNIT_INFO                   = 130,
    SEI_TEMPORAL_LEVEL0_INDEX                = 131,
    SEI_DECODED_PICTURE_HASH                 = 132,
    SEI_SCALABLE_NESTING                     = 133,
    SEI_REGION_REFRESH_INFO                  = 134,
    SEI_TIME_CODE                            = 136,
} SeiType;


typedef enum
{
    P_SLICE = 0,
    B_SLICE = 1,
    I_SLICE = 2,
    SP_SLICE = 3,
    SI_SLICE = 4,
    NUM_SLICE_TYPES = 5
} SliceType;


typedef enum
{
    PIC_STRUCT_FRAME,
    PIC_STRUCT_FIELD_TOP,
    PIC_STRUCT_FIELD_BOT,
} PicStruct;


enum
{
    LIST_0 = 0,
    LIST_1 = 1,
    BI_PRED = 2,
    BI_PRED_L0 = 3,
    BI_PRED_L1 = 4
};



typedef struct
{
    uint32_t u32Width;
    uint32_t u32Height;
} AvcInfo_t;


typedef struct
{
  uint32_t cpb_cnt_minus1;                                  // ue(v)
  uint32_t bit_rate_scale;                                  // u(4)
  uint32_t cpb_size_scale;                                  // u(4)
  uint32_t bit_rate_value_minus1 [MAXIMUMVALUEOFcpb_cnt];   // ue(v)
  uint32_t cpb_size_value_minus1 [MAXIMUMVALUEOFcpb_cnt];   // ue(v)
  uint32_t cbr_flag              [MAXIMUMVALUEOFcpb_cnt];   // u(1)
  uint32_t initial_cpb_removal_delay_length_minus1;         // u(5)
  uint32_t cpb_removal_delay_length_minus1;                 // u(5)
  uint32_t dpb_output_delay_length_minus1;                  // u(5)
  uint32_t time_offset_length;                              // u(5)
} HRD_t;


typedef struct
{
    bool        aspect_ratio_info_present_flag;             // u(1)
    uint8_t     aspect_ratio_idc;                           // u(8)
    uint16_t    sar_width;                                  // u(16)
    uint16_t    sar_height;                                 // u(16)
    bool        overscan_info_present_flag;                 // u(1)
    bool        overscan_appropriate_flag;                  // u(1)
    bool        video_signal_type_present_flag;             // u(1)
    uint8_t     video_format;                               // u(3)
    bool        video_full_range_flag;                      // u(1)
    bool        colour_description_present_flag;            // u(1)
    uint8_t     colour_primaries;                           // u(8)
    uint8_t     transfer_characteristics;                   // u(8)
    uint8_t     matrix_coefficients;                        // u(8)
    bool        chroma_location_info_present_flag;          // u(1)
    uint32_t    chroma_sample_loc_type_top_field;           // ue(v)
    uint32_t    chroma_sample_loc_type_bottom_field;        // ue(v)
    bool        timing_info_present_flag;                   // u(1)
    uint32_t    num_units_in_tick;                          // u(32)
    uint32_t    time_scale;                                 // u(32)
    bool        fixed_frame_rate_flag;                      // u(1)
    bool        nal_hrd_parameters_present_flag;            // u(1)
    HRD_t       nal_hrd_parameters;                         // hrd_paramters_t
    bool        vcl_hrd_parameters_present_flag;            // u(1)
    HRD_t       vcl_hrd_parameters;                         // hrd_paramters_t
    bool        low_delay_hrd_flag;                         // u(1)
    bool        pic_struct_present_flag;                    // u(1)
    bool        bitstream_restriction_flag;                 // u(1)
    bool        motion_vectors_over_pic_boundaries_flag;    // u(1)
    uint32_t    max_bytes_per_pic_denom;                    // ue(v)
    uint32_t    max_bits_per_mb_denom;                      // ue(v)
    uint32_t    log2_max_mv_length_vertical;                // ue(v)
    uint32_t    log2_max_mv_length_horizontal;              // ue(v)
    uint32_t    max_num_reorder_frames;                     // ue(v)
    uint32_t    max_dec_frame_buffering;                    // ue(v)
} VUI_t;


typedef struct
{
    bool isValid;

    uint8_t profile_idc;                                        // u(8)

    bool   constrained_set0_flag;                               // u(1)
    bool   constrained_set1_flag;                               // u(1)
    bool   constrained_set2_flag;                               // u(1)
    bool   constrained_set3_flag;                               // u(1)
    bool   constrained_set4_flag;                               // u(1)
    bool   constrained_set5_flag;                               // u(2)

    uint8_t level_idc;                                          // u(8)
    uint32_t seq_parameter_set_id;                              // ue(v)
    uint32_t chroma_format_idc;                                 // ue(v)

    uint32_t    bit_depth_luma_minus8;                          // ue(v)
    uint32_t    bit_depth_chroma_minus8;                        // ue(v)
    bool        qpprime_y_zero_transform_bypass_flag;           // u(1)

    bool    seq_scaling_matrix_present_flag;                    // u(1)
    bool    seq_scaling_list_present_flag[12];                  // u(1)
    int32_t ScalingList4x4[6][16];                              // se(v)
    int32_t ScalingList8x8[6][64];                              // se(v)
    bool    UseDefaultScalingMatrix4x4Flag[6];
    bool    UseDefaultScalingMatrix8x8Flag[6];

    uint32_t    log2_max_frame_num_minus4;                      // ue(v)
    uint32_t    pic_order_cnt_type;
    uint32_t    log2_max_pic_order_cnt_lsb_minus4;              // ue(v)
    bool        delta_pic_order_always_zero_flag;               // u(1)
    int32_t     offset_for_non_ref_pic;                         // se(v)
    int32_t     offset_for_top_to_bottom_field;                 // se(v)
    uint32_t    num_ref_frames_in_pic_order_cnt_cycle;          // ue(v)
    int32_t     offset_for_ref_frame[MAXnum_ref_frames_in_pic_order_cnt_cycle];   // se(v)
    uint32_t    max_num_ref_frames;                             // ue(v)
    bool        gaps_in_frame_num_value_allowed_flag;           // u(1)
    uint32_t    pic_width_in_mbs_minus1;                        // ue(v)
    uint32_t    pic_height_in_map_units_minus1;                 // ue(v)
    bool        frame_mbs_only_flag;                            // u(1)
    bool        mb_adaptive_frame_field_flag;                   // u(1)
    bool        direct_8x8_inference_flag;                      // u(1)
    bool        frame_cropping_flag;                            // u(1)
    uint32_t    frame_crop_left_offset;                         // ue(v)
    uint32_t    frame_crop_right_offset;                        // ue(v)
    uint32_t    frame_crop_top_offset;                          // ue(v)
    uint32_t    frame_crop_bottom_offset;                       // ue(v)
    bool        vui_parameters_present_flag;                    // u(1)
    VUI_t       vui_seq_parameters;                             // vui_seq_parameters_t
    bool        separate_colour_plane_flag;                     // u(1)
    int32_t     max_dec_frame_buffering;
    bool        lossless_qpprime_flag;
} SPS_t;


typedef struct
{
    bool isValid;

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
    bool   weighted_pred_flag;                                  // u(1)
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
} PPS_t;


typedef struct
{
    uint32_t    first_mb_in_slice;
    SliceType   slice_type;
    uint32_t    pic_parameter_set_id;
    uint8_t     colour_plane_id;
    uint16_t    frame_num;
    bool        field_pic_flag;
    bool        bottom_field_flag;
    uint32_t    idr_pic_id;
    uint32_t    pic_order_cnt_lsb;
    int32_t     delta_pic_order_cnt_bottom;
    int32_t     delta_pic_order_cnt[2];

    uint32_t redundant_pic_cnt;

    bool direct_spatial_mv_pred_flag;

    bool        num_ref_idx_active_override_flag;
    uint32_t    num_ref_idx_l0_active_minus1;
    uint32_t    num_ref_idx_l1_active_minus1;

    bool        ref_pic_list_modification_flag_l0;  
    bool        ref_pic_list_modification_flag_l1;
    std::vector< std::vector<uint32_t> > ref_pic_list_modification_q0;   // list0 of { modification_of_pic_num_idc, abs_diff_pic_num_minus1 / long_term_pic_num }
    std::vector< std::vector<uint32_t> > ref_pic_list_modification_q1;   // list1 of { modification_of_pic_num_idc, abs_diff_pic_num_minus1 / long_term_pic_num }
    
    uint32_t    luma_log2_weight_denom;
    uint32_t    chroma_log2_weight_denom;

    bool        luma_weight_l0_flag;
    int32_t     luma_weight_l0[MAX_REFERENCE_PICTURES];
    int32_t     luma_offset_l0[MAX_REFERENCE_PICTURES];

    bool        chroma_weight_l0_flag;
    int32_t     chroma_weight_l0[MAX_REFERENCE_PICTURES][2];
    int32_t     chroma_offset_l0[MAX_REFERENCE_PICTURES][2];

    bool        luma_weight_l1_flag;
    int32_t     luma_weight_l1[MAX_REFERENCE_PICTURES];
    int32_t     luma_offset_l1[MAX_REFERENCE_PICTURES];

    bool        chroma_weight_l1_flag;
    int32_t     chroma_weight_l1[MAX_REFERENCE_PICTURES][2];
    int32_t     chroma_offset_l1[MAX_REFERENCE_PICTURES][2];

    bool        no_output_of_prior_pics_flag;
    bool        long_term_reference_flag;
    bool        adaptive_ref_pic_marking_mode_flag;
    /*
     * list of { memory_management_control_operation, difference_of_pic_nums_minus1 / long_term_pic_num / long_term_frame_idx / max_long_term_frame_idx_plus1 }
     */
    std::vector< std::vector<uint32_t> > memory_management_control_ops;

    uint32_t    cabac_init_idc;

    int32_t     slice_qp_delta;
    bool        sp_for_switch_flag;
    int32_t     slice_qs_delta;

    uint32_t    disable_deblocking_filter_idc;
    int32_t     slice_alpha_c0_offset_div2;
    int32_t     slice_beta_offset_div2;
    uint32_t    slice_group_change_cycle;
} Slice_t;


#endif
