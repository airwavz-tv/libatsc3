//
//  atsc3_lls_sls_monitor_output_buffer.h
//  libatsc3
//
//  Created by Jason Justman on 3/2/19.
//  Copyright © 2019 Jason Justman. All rights reserved.
//


#include <stdbool.h>
//#include <arpa/inet.h>

#include "atsc3_utils.h"
#include "atsc3_player_ffplay.h"
#include "atsc3_mmtp_packet_types.h"
#include "atsc3_vector_builder.h"
#include "atsc3_isobmff_trun_box.h"

#ifndef ATSC3_LLS_SLS_MONITOR_OUTPUT_BUFFER_H
#define ATSC3_LLS_SLS_MONITOR_OUTPUT_BUFFER_H

#if defined (__cplusplus)
extern "C" {
#endif

#define _LLS_SLS_MONITOR_OUTPUT_MAX_INIT_BUFFER 65535
#define _LLS_SLS_MONITOR_OUTPUT_MAX_MOOF_BUFFER 65535
//4k gets pretty big pretty quick
#define _LLS_SLS_MONITOR_OUTPUT_MAX_FRAGMENT_BUFFER 16384000


typedef struct lls_sls_monitor_buffer_isobmff {
	uint16_t packet_id;
	uint32_t track_id;

	bool	 mpu_presentation_time_set;
	uint64_t mpu_presentation_time;
	uint32_t mpu_presentation_time_s;
	uint32_t mpu_presentation_time_us;

	uint32_t fps_num;
	uint32_t fps_denom;

    //isobmff init (ftyp and moov box)
	//inflight refragment, abandonded if there is packet loss
	block_t* init_block_flow_refragment;

	//current mpu init, kept in case of any packet loss
	block_t* init_block;

    //combined alc moof and mdat block for rOUTE
    block_t* alc_moof_mdat_block;

    //mmt - for building moof and mdat blocks
    ATSC3_VECTOR_BUILDER_STRUCT(trun_sample_entry);

    bool trun_mmthsample_missing_offset_mdat_box;
    
    block_t* mmt_moof_block;

    //ooo won't have flow until our mdat is complete
    block_t* mmt_moof_block_from_flow;

    //todo - not sure if neededooo won't have flow until our mdat is complete
    block_t* mmt_moof_block_previous_mpu;

    block_t* mmt_mdat_block;
    block_t* mmt_mpu_rebuilt_single;
    
    block_t* mmt_mpu_rebuilt_and_appending_for_isobmff_mux;

    //for fragment recovery
    //mmtp_payload_fragments_union_t* last_fragment;
    //this won't reset unless we do _all
    //mmtp_payload_fragments_union_t* last_mpu_sequence_number_last_fragment;

    //uint32_t last_fragment_lost_mfu_count;

} lls_sls_monitor_buffer_isobmff_t;


ATSC3_VECTOR_BUILDER_METHODS_INTERFACE(lls_sls_monitor_buffer_isobmff, trun_sample_entry)


void lls_sls_monitor_buffer_isobmff_clear_trun_sample_entry_instances(lls_sls_monitor_buffer_isobmff_t* lls_sls_monitor_buffer_isobmff);

    
typedef struct lls_sls_monitor_buffer {
    bool has_written_init_box;
    bool should_flush_output_buffer;
    
    lls_sls_monitor_buffer_isobmff_t audio_output_buffer_isobmff;
    lls_sls_monitor_buffer_isobmff_t video_output_buffer_isobmff;

    block_t* joined_isobmff_block;

} lls_sls_monitor_output_buffer_t;

//TODO: refactor me

typedef struct http_output_buffer {
	//used to signal pipe writer new data is present
	pthread_mutex_t* http_payload_buffer_mutex;

	block_t* http_payload_buffer_incoming;
	uint32_t total_fragments_incoming_written;

	block_t* http_payload_buffer_client_output;
	bool http_output_conntected;

	sig_atomic_t writer_unlock_count;

} http_output_buffer_t;

typedef struct lls_sls_monitor_buffer_mode {
    bool 	file_dump_enabled;
    bool 	ffplay_output_enabled;
    bool 	http_output_enabled;
    struct	pipe_ffplay_buffer* pipe_ffplay_buffer; //needed for circular deps
    http_output_buffer_t* http_output_buffer;


} lls_sls_monitor_output_buffer_mode_t;




#if defined (__cplusplus)
}
#endif


#endif /* ATSC3_LLS_SLS_MONITOR_OUTPUT_BUFFER_H */
