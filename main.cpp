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


#define ZEROBYTES_SHORTSTARTCODE    2
#define SIZE_OF_NAL_UNIT_HDR        1
#define NAL_HDR_MAX_SIZE            100
#define ES_BUFFER_SIZE              NAL_HDR_MAX_SIZE


static uint8_t u8endCode[] = { 0xFC, 0xFD, 0xFE, 0xFF };

static uint8_t u8EsBuffer[ES_BUFFER_SIZE + sizeof(u8endCode)];

static AvcInfo_t tAvcInfo;

static SPS_t SPSs[32];

static PPS_t PPSs[128];

static Slice_t slice;

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


static bool isOk(InputBitstream_t &ibs, OutputBitstream_t &obs)
{   
    for (int i = 0; i < obs.m_fifo.size(); i++)
    {
        if (ibs.m_fifo[i] != obs.m_fifo[i]) { return false; }
    }

    return true;
}

/*!
************************************************************************
*  \brief
*     This function add emulation_prevention_three_byte for all occurrences
*     of the following byte sequences in the stream
*       0x000000  -> 0x00000300
*       0x000001  -> 0x00000301
*       0x000002  -> 0x00000302
*       0x000003  -> 0x00000303
*
*  \param NaluBuffer
*            pointer to target buffer
*  \param rbsp
*            pointer to source buffer
*  \param rbsp_size
*           Size of source
*  \return
*           Size target buffer after emulation prevention.
*
************************************************************************
*/
int RBSPtoEBSP(vector<uint8_t> &ebsp, vector<uint8_t> &rbsp)
{
    int zero_cnt    = 0;

    for (int i = 0; i < rbsp.size(); i++)
    {
        if (zero_cnt == ZEROBYTES_SHORTSTARTCODE && !(rbsp[i] & 0xFC))
        {
            ebsp.push_back(0x03);
            zero_cnt = 0;
        }
        ebsp.push_back(rbsp[i]);

        if (rbsp[i] == 0x00) { zero_cnt++; }
        else { zero_cnt = 0; }
    }

    return ebsp.size();
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
        perror(argv[1]);
        exit(-1);
    }

    uint8_t *data = (uint8_t *) calloc(1, file_size + sizeof(u8EsBuffer));

    rd_sz = read(fd, data, file_size);

    uint8_t     nal_unit_header[SIZE_OF_NAL_UNIT_HDR];
    bool        forbidden_zero_bit;
    uint8_t     nal_ref_idc;
    NaluType    nal_unit_type;

    uint8_t *ptr = data;

    while (ptr - data < file_size)
    {
        bool     nalFound = false;
        uint32_t prefix_len = 0;

        if (has_start_code(ptr, 2))
        {
            prefix_len = 3;
            nal_unit_header[0] = ptr[prefix_len];
            nalFound = true;
        }
        else if (has_start_code(ptr, 3))
        {
            prefix_len = 4;
            nal_unit_header[0] = ptr[prefix_len];
            nalFound = true;
        }

        if (nalFound)
        {
            nal_unit_type           = (NaluType) ((nal_unit_header[0] & (BIT4 | BIT3 | BIT2 | BIT1 | BIT0)));
            nal_ref_idc             = ((nal_unit_header[0] & (BIT5 | BIT6)) >> 5);
            forbidden_zero_bit      = (nal_unit_header[0] & BIT7) >> 7;

            if (!forbidden_zero_bit)
            {
                printf("nal=0x%02x forbidden_zero_bit=%d, nal_unit_type=%02u, nal_ref_idc=%u, offset=0x%x\n",
                       nal_unit_header[0],
                       forbidden_zero_bit,
                       nal_unit_type,
                       nal_ref_idc,
                       (uint32_t) (ptr - data));

                memcpy(u8EsBuffer, ptr, sizeof(u8EsBuffer));

                InputBitstream_t ibs;

                ibs.m_fifo          = &u8EsBuffer[prefix_len + SIZE_OF_NAL_UNIT_HDR];
                ibs.m_fifo_size     = sizeof(u8EsBuffer) - (prefix_len + SIZE_OF_NAL_UNIT_HDR);
                ibs.m_fifo_idx      = 0;
                ibs.m_num_held_bits = 0;
                ibs.m_held_bits     = 0;
                ibs.m_numBitsRead   = 0;

                EBSPtoRBSP(ibs.m_fifo, ibs.m_fifo_size, 0);

                switch (nal_unit_type)
                {
                    case NALU_TYPE_SPS:
                    {
                        printf("Find SPS, parse!\n");

                        ParseSPS(ibs, SPSs, tAvcInfo);
                        {
                            OutputBitstream_t obs;

                            obs.m_num_held_bits = 0;
                            obs.m_held_bits     = 0;

                            if (SPSs[0].isValid) // assume sps id is 0
                            {
                                SPSs[0].log2_max_frame_num_minus4 = 11; // do customer request, generate SPS log2_max_frame_num = 15

                                printf("Generating SPS!\n");
                                GenerateSPS(obs, SPSs[0]);

                                if (obs.m_fifo.size() != ibs.m_fifo_idx)
                                {
                                    printf("Generated SPS len is different! %ld:%d\n", obs.m_fifo.size(), ibs.m_fifo_idx);
                                    exit(-1);
                                }
                                
                                SPSs[0].log2_max_frame_num_minus4 = 12; // adjust back because we use 12 to parse slice


                                //printf("\n\n--");
                                //for (int i = 0; i < obs.m_fifo.size(); i++)
                                //{
                                //    printf("0x%02x ", obs.m_fifo[i]);
                                //}
                                //printf("--\n\n");

                                //InputBitstream_t ibs0;

                                //ibs0.m_fifo_size    = sizeof(u8EsBuffer) - (prefix_len + SIZE_OF_NAL_UNIT_HDR);
                                //ibs0.m_fifo         = (uint8_t *) calloc(1, ibs0.m_fifo_size);
                                //ibs0.m_fifo_idx      = 0;
                                //ibs0.m_num_held_bits = 0;
                                //ibs0.m_held_bits     = 0;
                                //ibs0.m_numBitsRead   = 0;

                                //for (int i = 0; i < obs.m_fifo.size(); i++)
                                //{
                                //    ibs0.m_fifo[i] = obs.m_fifo[i];
                                //}

                                //SPS_t sps0;
                                //AvcInfo_t aaa;
                                //ParseSPS(ibs0, SPSs, aaa);

                                vector<uint8_t> ebsp;
                                
                                RBSPtoEBSP(ebsp, obs.m_fifo);

                                ebsp.insert(ebsp.begin(), ptr, ptr+prefix_len+1);
                                for (int i = 0; i < ebsp.size(); i++)
                                {
                                    ptr[i] = ebsp[i];
                                }

                                //printf("\n\n--EBSP: ");
                                //for (int i = 0; i < ebsp.size(); i++)
                                //{
                                //    printf("0x%02x ", ebsp[i]);
                                //}
                                //printf("--\n\n");

                                //exit(0);
                            }
                        }
                        break;
                    }
                    case NALU_TYPE_PPS:
                    {
                        ParsePPS(ibs, PPSs, SPSs);

                        break;
                    }
                    case NALU_TYPE_AUD:
                    {
                        //ParseAUD(ibs);

                        break;
                    }
                    case NALU_TYPE_IDR:
                    case NALU_TYPE_SLICE:
                    {
                        static int cnt = 0;
                        static int error_cnt = 0;
                        
                        bool IdrPicFlag = ( ( nal_unit_type == 5 ) ? 1 : 0 );
                        int ret = ParseSlice(ibs, slice, SPSs, PPSs, IdrPicFlag, nal_ref_idc, message);

                        if (ret < 0)
                        {
                        }
                        else
                        {
                            SPS_t x_sps = SPSs[ PPSs[slice.pic_parameter_set_id].seq_parameter_set_id ];
                            x_sps.log2_max_frame_num_minus4 = 11;

                            slice.frame_num %= (1 << 15);
                        
                            OutputBitstream_t obs;

                            obs.m_num_held_bits = 0;
                            obs.m_held_bits     = 0;

                            GenerateSlice(obs, slice, x_sps, PPSs[slice.pic_parameter_set_id], IdrPicFlag, nal_ref_idc);

                            if (obs.m_fifo.size() == ibs.m_fifo_idx)
                            {
                                obs.m_fifo.insert(obs.m_fifo.begin(), ptr, ptr+prefix_len+1);
                                for (int i = 0; i < obs.m_fifo.size(); i++)
                                {
                                    ptr[i] = obs.m_fifo[i];
                                }
                            }
                            else
                            {
                                printf("Generated slice len is different! %ld:%d\n", obs.m_fifo.size(), ibs.m_fifo_idx);

                                //for (int i = 0; i < prefix_len+1+ibs.m_fifo_idx; i++)
                                //{
                                //    printf("0x%02x ", ptr[i]);
                                //}
                                //printf("\n");

                                // Padding 0x00 to make slice len the same, so it becomes [0x00 + prefix + NAL header + slice header]
                                obs.m_fifo.insert(obs.m_fifo.begin(), ptr, ptr+prefix_len+1);
                                obs.m_fifo.insert(obs.m_fifo.begin(), {0x00});
                                for (int i = 0; i < obs.m_fifo.size(); i++)
                                {
                                    ptr[i] = obs.m_fifo[i];
                                }
                                //for (int i = 0; i < obs.m_fifo.size(); i++)
                                //{
                                //    printf("0x%02x ", obs.m_fifo[i]);
                                //}
                                //printf("\n");
                                
                                //exit(-1);
                            }
                        }
                        
                        break;
                    }
                    case NALU_TYPE_SEI:
                    {
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

    // Flush output
    {
        char output[256];
        char *cp = strrchr(argv[1], '.');

        strncpy(output, argv[1], cp - argv[1]);
        strcat(output, "_fix_frame_num");
        strcat(output, cp);

        int ofd = open(output, O_RDWR | O_CREAT, S_IRUSR);
        write(ofd, data, file_size);
        close(ofd);
    }

    return 0;
}

