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



void ParseSPS(InputBitstream_t &bitstream, AvcInfo_t *pAvcInfo)
{
}


void ParsePPS(InputBitstream_t &bitstream)
{
}


void ParseSliceHeader(InputBitstream_t &bitstream, NaluType nal_type, string &message)
{
}