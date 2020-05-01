//
// Created by Jason Justman on 2019-11-23.
//

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <queue>

// Airwavz.tv RedZone Receiver SDK includes
#include "props_api.h"
#include "redzone_c_api.h"
#include "basebandparser.h"

using namespace std;

#ifndef LIBATSC3_ATSC3NDKCLIENTAIRWAVZRZR_H
#define LIBATSC3_ATSC3NDKCLIENTAIRWAVZRZR_H

#include <atsc3_utils.h>

#include <atsc3_sl_tlv_demod_type.h>
#include <atsc3_alp_parser.h>

#include "atsc3NdkClient.h"

typedef void * (*THREADFUNCPTR)(void *);

typedef struct atsc3RedZoneParserCallbackData
{
    enum RedZoneOperatingMode device_mode;
} atsc3RedZoneParserCallbackData_t;

class atsc3NdkClientAirwavzRZR {
    public:
        void Init(atsc3NdkClient* ref_);

        int Open(int fd, int bus, int addr);
        int Tune(int freqKhz, int plpId);
        int TuneMultiplePLP(int freqKhz, vector<int> plpIds);
        int Close();

        int GetTunerStatus();


        void processTLVFromCallback();

        //airwavz specific PHY callbacks
        //static void alpParserIPv4Callback(uint32_t plpId, const uint8_t *pPacket, int32_t sPacket, int32_t SID, void *pUserData);
        static void basebandParserALPCallback(uint32_t plpId, const uint8_t *pPacket, int32_t sPacket, void *pUserData);
        //static void alpParserLLSCallback(uint32_t plpId, const uint8_t *pPacket, int32_t sPacket, void *pUserData);
        static void redZoneCaptureBasebandPacketCallback(RedZoneCaptureBasebandPacket *pPacket, void *pUserData);


        atsc3RedZoneParserCallbackData_t    atsc3RedZoneParserCallbackData;
        //static RZRALPParserHandle_t         hALPParser;
        static RZRBasebandParserHandle_t    hBasebandParser;
        //end airwavz specific PHY callbacks


    //thread state management flags, TODO: remove from static to instance binding
     //   static bool        captureThreadShouldRun;
     //   static bool        processThreadShouldRun;
        static bool        tunerStatusThreadShouldRun;
        static bool        tunerStatusThreadShouldPollTunerStatus;

        //tuner/sl unit global values, TODO: move to instance values
        static int         global_tUnit;  //tUnint, move to instance
        static int         global_slUnit; //jjustman-2019-12-18 remove me

    static void RxDataCallback(unsigned char *data, long len);

    //data metrics
    uint64_t alp_completed_packets_parsed;
    uint64_t alp_total_bytes;
    uint64_t alp_total_LMTs_recv;

private:

    static atsc3NdkClient* atsc3NdkClient_ref;   //reference to our base JNI NDI handler

    //thread handler reference Tuner Status thread
    pthread_t   tsThreadID = 0;

    //thread handling method
    static void* TunerStatusThread(void*);

    //libusb instance device values
    int fd;
 //   int bus;
 //   int addr;


    void resetProcessThreadStatistics();
};

#endif //LIBATSC3_ATSC3NDKCLIENTNOPHYIMPL_H
