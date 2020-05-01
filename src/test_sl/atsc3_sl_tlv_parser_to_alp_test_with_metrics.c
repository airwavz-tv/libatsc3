/*
 * atsc3_sl_tlv_parser_to_alp_test_with_metrics.c
 *
 *  Created on: Nov 23, 2019
 *      Author: jjustman
 */

#include <stdio.h>
#include <string.h>

#include "../atsc3_utils.h"
#include "../atsc3_sl_tlv_demod_type.h"


#define _ATSC3_SL_TLV_PARSER_TO_ALP_TEST_ERROR(...)   printf("%s:%d:ERROR:",__FILE__,__LINE__);_ATSC3_UTILS_PRINTLN(__VA_ARGS__);
#define _ATSC3_SL_TLV_PARSER_TO_ALP_TEST_WARN(...)    printf("%s:%d:WARN:",__FILE__,__LINE__);_ATSC3_UTILS_PRINTLN(__VA_ARGS__);
#define _ATSC3_SL_TLV_PARSER_TO_ALP_TEST_INFO(...)    printf("%s:%d:INFO:",__FILE__,__LINE__);_ATSC3_UTILS_PRINTLN(__VA_ARGS__);
#define _ATSC3_SL_TLV_PARSER_TO_ALP_TEST_DEBUG(...)   printf("%s:%d:DEBUG:",__FILE__,__LINE__);_ATSC3_UTILS_PRINTLN(__VA_ARGS__);

FILE* atsc3_sl_tlv_open_filename(const char* sl_tlv_filename) {
	FILE* atsc3_sl_tlv_fp = NULL;

	struct stat st;
	stat(sl_tlv_filename, &st);
	if(!st.st_size) {
		_ATSC3_SL_TLV_PARSER_TO_ALP_TEST_ERROR("atsc3_sl_tlv_open_filename: %s, ERROR: st.st_size is 0!", sl_tlv_filename);
		return NULL;
	}

	atsc3_sl_tlv_fp = fopen(sl_tlv_filename, "r");
	if(!atsc3_sl_tlv_fp) {
		_ATSC3_SL_TLV_PARSER_TO_ALP_TEST_ERROR("atsc3_sl_tlv_open_filename: %s -  fopen() for read returned NULL!", sl_tlv_filename);
		return NULL;
	}

	return atsc3_sl_tlv_fp;
}

#define SL_TLV_BLOCK_SIZE 65535
#define SL_TLV_REPLAY_TEST_FILENAME_DEFAULT "testdata/2019-11-22-647mhz.tlv.bin"

int main(int argc, char* argv[] ) {
	__ATSC3_SL_TLV_USE_INLINE_ALP_PARSER_CALL__ = 1;

	char* SL_TLV_REPLAY_TEST_FILENAME = NULL;
	
	if(argc == 2) {
		//load our file from the command line parameters
		SL_TLV_REPLAY_TEST_FILENAME = argv[1];
	} else {
		SL_TLV_REPLAY_TEST_FILENAME = SL_TLV_REPLAY_TEST_FILENAME_DEFAULT;
	}

    FILE* atsc3_sl_tlv_fp = atsc3_sl_tlv_open_filename(SL_TLV_REPLAY_TEST_FILENAME);
	if(!atsc3_sl_tlv_fp) {
		_ATSC3_SL_TLV_PARSER_TO_ALP_TEST_ERROR("Unable to open filename: %s", SL_TLV_REPLAY_TEST_FILENAME);
		exit(1);
	}
	
    _ATSC3_SL_TLV_PARSER_TO_ALP_TEST_DEBUG("Opening SL TLV.bin: %s", SL_TLV_REPLAY_TEST_FILENAME);
	atsc3_sl_tlv_payload_metrics_t* atsc3_sl_tlv_payload_metrics = calloc(1, sizeof(atsc3_sl_tlv_payload_metrics_t));

    uint8_t* buf = calloc(SL_TLV_BLOCK_SIZE, sizeof(uint8_t));
    block_t* atsc3_sl_tlv_block = block_Alloc(0);
    atsc3_sl_tlv_payload_t* atsc3_sl_tlv_payload = NULL;
	
    int bytes_read = 0;

    if(atsc3_sl_tlv_fp) {
        while(!feof(atsc3_sl_tlv_fp)) {
            _ATSC3_SL_TLV_PARSER_TO_ALP_TEST_DEBUG("fread: position: %ld", ftell(atsc3_sl_tlv_fp));

        	bytes_read = fread(buf, 1, 65535, atsc3_sl_tlv_fp);
			atsc3_sl_tlv_payload_metrics->total_tlv_bytes_read += bytes_read;
			
        	if(bytes_read > 0) {
        		block_Write(atsc3_sl_tlv_block, buf, bytes_read);
        		block_Rewind(atsc3_sl_tlv_block);

        		do {
        			atsc3_sl_tlv_payload = atsc3_sl_tlv_payload_parse_from_block_t_with_metrics(atsc3_sl_tlv_block, atsc3_sl_tlv_payload_metrics);

        			if(atsc3_sl_tlv_payload) {
        				atsc3_sl_tlv_payload_dump(atsc3_sl_tlv_payload);
        				if(atsc3_sl_tlv_payload->alp_payload_complete) {
							atsc3_sl_tlv_payload_metrics->total_tlv_packets_parsed_count++;
        				}
        			}

        		} while(atsc3_sl_tlv_payload && atsc3_sl_tlv_payload->alp_payload_complete);

        		if(atsc3_sl_tlv_payload && !atsc3_sl_tlv_payload->alp_payload_complete && atsc3_sl_tlv_block->i_pos > atsc3_sl_tlv_payload->sl_tlv_total_parsed_payload_size) {
        			//accumulate from our last starting point and continue iterating over replay test filename
        			atsc3_sl_tlv_block->i_pos -= atsc3_sl_tlv_payload->sl_tlv_total_parsed_payload_size;
        		}

        		if(atsc3_sl_tlv_block->p_size > atsc3_sl_tlv_block->i_pos) {
        			//truncate our current block_t starting at i_pos, and then read next i/o block
        			block_t* temp_sl_tlv_block = block_Duplicate_from_position(atsc3_sl_tlv_block);
					block_Destroy(&atsc3_sl_tlv_block);
					atsc3_sl_tlv_block = temp_sl_tlv_block;
					block_Seek(atsc3_sl_tlv_block, atsc3_sl_tlv_block->p_size);
        		} else {
                    _ATSC3_SL_TLV_PARSER_TO_ALP_TEST_DEBUG("atsc3_sl_tlv_block: positioning mismatch: i_pos: %d, p_size: %d", atsc3_sl_tlv_block->i_pos, atsc3_sl_tlv_block->p_size);

        			//jjustman: 2019-11-23: todo: try seek for our magic packet here
        			block_Rewind(atsc3_sl_tlv_block);
        		}
        	}
        }
    }

	_ATSC3_SL_TLV_PARSER_TO_ALP_TEST_DEBUG("Completed parsing of SL TLV.bin: %s",
											SL_TLV_REPLAY_TEST_FILENAME);
	
	atsc3_sl_tlv_payload_metrics_dump(atsc3_sl_tlv_payload_metrics);
	
    return 0;
}

