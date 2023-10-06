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


void ParseSPS(InputBitstream_t &bitstream, AvcInfo_t *pAvcInfo)
{
    uint8_t profile_idc;

    uint8_t constraint_set0_flag;
    uint8_t constraint_set1_flag;
    uint8_t constraint_set2_flag;
    uint8_t constraint_set3_flag;
    uint8_t constraint_set4_flag;
    uint8_t constraint_set5_flag;

    uint8_t level_idc;

    uint32_t chroma_format_idc;
    bool separate_colour_plane_flag;
    
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

    profile_idc = READ_CODE(bitstream, 8, "profile_idc");

    constraint_set0_flag = READ_FLAG(bitstream, "constraint_set0_flag");
    constraint_set1_flag = READ_FLAG(bitstream, "constraint_set1_flag");
    constraint_set2_flag = READ_FLAG(bitstream, "constraint_set2_flag");
    constraint_set3_flag = READ_FLAG(bitstream, "constraint_set3_flag");
    constraint_set4_flag = READ_FLAG(bitstream, "constraint_set4_flag");
    constraint_set5_flag = READ_FLAG(bitstream, "constraint_set5_flag");

    READ_CODE(bitstream, 2, "reserved_zero_2bits");

    level_idc = READ_CODE(bitstream, 8, "level_idc");

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
                seq_scaling_list_present_flag[i] = READ_FLAG(bitstream, "seq_scaling_list_present_flag");
                if (seq_scaling_list_present_flag[i])
                {
                    if (i < 6)
                    {
                        scaling_list(bitstream, ScalingList4x4[ i ], 16, UseDefaultScalingMatrix4x4Flag[ i ] );
                    }
                    else
                    {
                        scaling_list(bitstream, ScalingList8x8[ i - 6 ], 64, UseDefaultScalingMatrix8x8Flag[ i - 6 ] );
                    }
                }
            }
        }
    }

    log2_max_frame_num_minus4 = READ_UVLC(bitstream, "log2_max_frame_num_minus4");
}


void ParsePPS(InputBitstream_t &bitstream)
{
}


void ParseSliceHeader(InputBitstream_t &bitstream, NaluType nal_type, string &message)
{
}