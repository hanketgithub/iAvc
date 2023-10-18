//
//  main.cpp
//  iAvc
//
//  Created by Hank Lee on 2023/10/05.
//  Copyright (c) 2023 Hank Lee. All rights reserved.
//

/******************************
 * include
 */
#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <iostream>
#include <string>
#include <vector>
      
#include "common.h"
#include "bits.h"
#include "parser.h"
#include "writer.h"


using namespace std;


#define SIZE_OF_NAL_UNIT_HDR    1
#define ES_BUFFER_SIZE          (3840 * 2160)

static uint8_t u8endCode[] = { 0xFC, 0xFD, 0xFE, 0xFF };

static uint8_t u8EsBuffer[ES_BUFFER_SIZE + sizeof(u8endCode)];

static AvcInfo_t tAvcInfo;

static SPS_t sps;

string message;

/******************************
 * local function
 */
 
static bool has_start_code
(
    uint8_t *addr,
    uint8_t  zeros
)
{
    int i;
    
    for (i = 0; i < zeros; i++)
    {
        if (addr[i]) return false;
    }
    
    return addr[i] == 0x01 ? true : false;
}


static bool has_end_code(uint8_t *p)
{
    if (memcmp(p, u8endCode, sizeof(u8endCode)) == 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}


static bool scan_nal
(
    uint8_t     *start_addr,
    uint8_t     *nal_unit_header,
    uint32_t    *nal_len,
    uint32_t    *prefix_len
)
{
    uint8_t offset = 0;
    uint8_t *p;

    bool ret = true;
    
    p = start_addr;

    if (has_start_code(p, 2))      // short prefix
    {
        offset = 3;
    }
    else if (has_start_code(p, 3)) // long prefix
    {
        offset = 4;
    }

    //printf("prefix offset=%d\n", offset);
    *prefix_len = offset;
    p += offset;

    while (!has_end_code(p) && !has_start_code(p, 2) && !has_start_code(p, 3))
    {
        p++;
    }

    nal_unit_header[0] = start_addr[offset];
    *nal_len  = (uint32_t) (p - start_addr);

    if (has_end_code(p))
    {
        ret = false;
    }

    return ret;
}

static uint32_t EBSPtoRBSP
(
    uint8_t *streamBuffer,
    uint32_t end_bytepos,
    uint32_t begin_bytepos
)
{
    uint32_t i;
    uint32_t j;
    uint32_t count;
    
    count = 0;
    j = begin_bytepos;
    
    for (i = begin_bytepos; i < end_bytepos; i++)
    {
        // in NAL unit, 0x000000, 0x000001, 0x000002 shall not occur at any byte-aligned position
        if (count == 2 && streamBuffer[i] < 0x03)
        {
            return -1;
        }
        
        if (count == 2 && streamBuffer[i] == 0x03)
        {
            // check the 4th byte after 0x000003, except when cabac.....
            if ((i < end_bytepos - 1) && (streamBuffer[i + 1] > 0x03))
            {
                return -1;
            }
            
            if (i == end_bytepos - 1)
            {
                return j;
            }
            
            // escape 0x03 byte!
            i++;
            count = 0;
        }
        
        streamBuffer[j] = streamBuffer[i];
        //printf("[%02u] 0x%02x\n", j, streamBuffer[j]);
        
        if (streamBuffer[i] == 0x00)
        {
            count++;
        }
        else
        {
            count = 0;
        }
        
        j++;
    }
    
    return j;
}


int main(int argc, char *argv[])
{
    int fd;
    ssize_t rd_sz;
        
    if (argc < 2)
    {
        printf("useage: %s [input_file]\n", argv[0]);
        
        return -1;
    }

    fd = open(argv[1], O_RDONLY);
    if (fd < 0)
    {
        perror(argv[1]);
        exit(-1);
    }

    struct stat st;
    uint32_t file_size = 0;

    if (stat(argv[1], &st) == 0)
    {
        file_size = st.st_size;
    }
    else
    {
        exit(-1);
    }

    uint8_t *buf = (uint8_t *) calloc(1, file_size + sizeof(u8EsBuffer));

    rd_sz = read(fd, buf, file_size);

    //printf("file_size=%x rd_size=%x\n", file_size, rd_sz);

    uint8_t     nal_unit_header[SIZE_OF_NAL_UNIT_HDR];
    bool        forbidden_zero_bit;
    uint8_t     nal_ref_idc;
    NaluType    nal_unit_type;

    uint8_t *ptr = buf;

    while (ptr - buf < file_size)
    {
        bool     nalFound = false;
        uint32_t prefix_len = 0;

        if (has_start_code(ptr, 2))
        {
            prefix_len = 3;

            // 0x00 0x00 0x01 0x47 is TS header
            if (ptr[prefix_len] == 0x47)
            {
                ptr += prefix_len;
                continue;
            }

            nal_unit_header[0] = ptr[prefix_len];
            nalFound = true;
        }
        else if (has_start_code(ptr, 3))
        {
            prefix_len = 4;

            // 0x00 0x00 0x00 0x01 0x47 is TS header
            if (ptr[prefix_len] == 0x47)
            {
                ptr += prefix_len;
                continue;
            }

            nal_unit_header[0] = ptr[prefix_len];
            nalFound = true;
        }

        if (nalFound)
        {
            nal_unit_type           = (NaluType) ((nal_unit_header[0] & (BIT4 | BIT3 | BIT2 | BIT1 | BIT0)));
            nal_ref_idc             = (nal_unit_header[0] & (BIT5 | BIT6) >> 5);
            forbidden_zero_bit      = (nal_unit_header[0] & BIT7) >> 7;

            if (!forbidden_zero_bit)
            {
                printf("nal=0x%02x forbidden_zero_bit=%d, nal_unit_type=%02u, nal_ref_idc=%u, offset=0x%x\n",
                       nal_unit_header[0],
                       forbidden_zero_bit,
                       nal_unit_type,
                       nal_ref_idc,
                       (uint32_t) (ptr - buf));

                memcpy(u8EsBuffer, ptr, sizeof(u8EsBuffer));

                InputBitstream_t bitstream;

                bitstream.m_fifo            = &u8EsBuffer[prefix_len + SIZE_OF_NAL_UNIT_HDR];
                bitstream.m_fifo_size       = sizeof(u8EsBuffer);
                bitstream.m_fifo_idx        = 0;
                bitstream.m_num_held_bits   = 0;
                bitstream.m_held_bits       = 0;
                bitstream.m_numBitsRead     = 0;

                switch (nal_unit_type)
                {
                    case NALU_TYPE_SPS:
                    {              
                        EBSPtoRBSP(&u8EsBuffer[prefix_len + SIZE_OF_NAL_UNIT_HDR], sizeof(u8EsBuffer), 0);
                        
                        ParseSPS(bitstream, sps, tAvcInfo);
                        
                        break;
                    }
                    case NALU_TYPE_PPS:
                    {
                        EBSPtoRBSP(&u8EsBuffer[prefix_len + SIZE_OF_NAL_UNIT_HDR], sizeof(u8EsBuffer), 0);
                        
                        ParsePPS(bitstream);
                        
                        break;
                    }
                    case NALU_TYPE_AUD:
                    {
                        EBSPtoRBSP(&u8EsBuffer[prefix_len + SIZE_OF_NAL_UNIT_HDR], sizeof(u8EsBuffer), 0);

                        //ParseAUD(bitstream);
                        
                        break;
                    }
                    case NALU_TYPE_SLICE:
                    {
                        EBSPtoRBSP(&u8EsBuffer[prefix_len + SIZE_OF_NAL_UNIT_HDR], sizeof(u8EsBuffer), 0);

                        //cout << "log2_max_frame_num_minus4=" << sps.log2_max_frame_num_minus4 << endl;

                        ParseSliceHeader(bitstream, sps, message);

                        break;
                    }
                    case NALU_TYPE_SEI:
                    {
                        EBSPtoRBSP(&u8EsBuffer[prefix_len + SIZE_OF_NAL_UNIT_HDR], sizeof(u8EsBuffer), 0);
                        
                        break;
                    }
                    default:
                    {
                        break;
                    }
                }
            }
        }

        if (prefix_len)
        {
            ptr += prefix_len;
        }
        else
        {
            ptr++;
        }
    }

    return 0;
}

