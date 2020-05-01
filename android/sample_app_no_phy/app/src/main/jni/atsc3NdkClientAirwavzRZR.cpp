//
// Created by Jason Justman on 2019-11-23.
//

#include <atsc3_utils.h>
#include <atsc3_phy_mmt_player_bridge.h>
#include "atsc3NdkClient.h"

#include "atsc3NdkClientAirwavzRZR.h"

// Airwavz.tv RedZone Receiver SDK includes
#include "props_api.h"
#include "redzone_c_api.h"
#include "basebandparser.h"

extern atsc3NdkClientAirwavzRZR apiImpl;

bool atsc3NdkClientAirwavzRZR::tunerStatusThreadShouldRun = false;
bool atsc3NdkClientAirwavzRZR::tunerStatusThreadShouldPollTunerStatus = false;

RedZoneCaptureHandle_t      hRedZoneCapture = nullptr;
RZRBasebandParserHandle_t   atsc3NdkClientAirwavzRZR::hBasebandParser = nullptr;
atsc3NdkClient* atsc3NdkClientAirwavzRZR::atsc3NdkClient_ref = NULL;
bool                        fRedZoneCaptureStarted = false;

//NDK JNI main dispatcher reference
void atsc3NdkClientAirwavzRZR::Init(atsc3NdkClient* ref_) {
    atsc3NdkClientAirwavzRZR::atsc3NdkClient_ref = ref_;
}

int atsc3NdkClientAirwavzRZR::Open(int fd_, int bus_, int addr_) {
    fd = fd_;
//    bus = bus_;
//    addr = addr_;
    int     rval;
    int     drv_verbosity = 2;      // suggest a value of 2 (0 - 9 legal) to set debug output level;

    //printf( "atsc3NdkClientAirwavzRZR Entered\n");

    if (RedZoneCaptureOpen(&hRedZoneCapture)) {
        printf("RedZoneCaptureOpen: Failed to initialize RedZoneCapture status = %d\n", rval);
        return 1;
    }
    rval = RedZoneCaptureSetProp(hRedZoneCapture, RedZoneLoggingVerboseMode, &drv_verbosity, sizeof(drv_verbosity));

    rval = RedZoneCaptureInitSysDevice(hRedZoneCapture, fd );
    if (rval != RZR_SUCCESS)
    {
        printf("RedZoneCaptureInitSysDevice: Failed to initialize RedZoneCapture %d\nThis could be a result on a firmware download and re-ennumeration - try again\n", rval);
        return 1;
    }

    RedZoneOperatingMode opmode = OperatingModeATSC3;
    atsc3RedZoneParserCallbackData.device_mode = opmode;

    rval = RedZoneCaptureSetProp(hRedZoneCapture, RedZoneOperatingModeProp, &opmode, sizeof(opmode));

    if(RZRBasebandParserOpen(&hBasebandParser)) {
        printf("Failed to open hBasebandParser");
        return -4;
    }

    if(RZRBasebandParserRegisterCallbacks(hBasebandParser, basebandParserALPCallback, &atsc3RedZoneParserCallbackData))
    {
        printf("Failed to register vendor packet parser callbacks");
        return -6;
    }

    if (RedZoneCaptureRegisterCallbacks(hRedZoneCapture, redZoneCaptureBasebandPacketCallback, &atsc3RedZoneParserCallbackData))
    {
        printf("Failed to register callbacks with RedZoneMemoryConsumer\n");
        return -7;
    }

    if (RedZoneCaptureStart(hRedZoneCapture))
    {
        printf("Failed to start RedZoneCapture\n");
        return -31337;
    }
    fRedZoneCaptureStarted = true;
    printf( "atsc3NdkClientAirwavzRZR Open:  success\n");
    return 0;
}

int atsc3NdkClientAirwavzRZR::Tune(int freqKhz, int plpId) {

    vector<int> myPlps;
    myPlps.push_back(plpId);

    return TuneMultiplePLP(freqKhz, myPlps);
}

/**
 * jjustman-2019-12-17: TODO - refactor this to InitTunerAndDemodTransfer
 * @param freqKhz
 * @param plpIds
 * @return
 */
int atsc3NdkClientAirwavzRZR::TuneMultiplePLP(int freqKhz, vector<int> plpIds) {

    int rval;

    int sThread_ret = 0;

    //1. config plp's
    //2. set tuner frequency
    //3. start status reporting thread

    printf( "atsc3NdkClientAirwavzRZR::TuneMultiplePLP freq = %d\n", freqKhz);

    rval = RedZoneCaptureSetProp(hRedZoneCapture, RedZoneFrequencyKHzProp, &freqKhz, sizeof(freqKhz));
    printf("tune returned %d\n", rval);

    RedZonePLPSet plpset = {255, 255, 255, 255};

    int nplp = plpIds.size();

    if (nplp > 4) {
        printf("Requested %d PLPs - only the first 4 will be tuned\n", nplp);
        nplp = 4;
    }
    switch (nplp) {

        case 0:
            printf("No PLPs specified\n");
            break;
        case 4:
            plpset.plp3_id = plpIds[3];
            RZRBasebandParserEnablePLP(hBasebandParser, plpset.plp3_id);
            /* fall through */
        case 3:
            plpset.plp2_id = plpIds[2];
            RZRBasebandParserEnablePLP(hBasebandParser, plpset.plp2_id);
            /* fall through */
        case 2:
            plpset.plp1_id = plpIds[1];
            RZRBasebandParserEnablePLP(hBasebandParser, plpset.plp1_id);
            /* fall through */
        case 1:
            plpset.plp0_id = plpIds[0];
            RZRBasebandParserEnablePLP(hBasebandParser, plpset.plp0_id);
            /* fall through */
            break;
    }
    if (nplp >0) {
        rval = RedZoneCaptureSetProp(hRedZoneCapture, RedZonePLPSelectionProp, &plpset, sizeof(plpset));
        //printf("Set PLP Selection returned %d\n", rval);
    }

    if (!tsThreadID) {

        atsc3NdkClientAirwavzRZR::tunerStatusThreadShouldRun = true;

        sThread_ret = pthread_create(&tsThreadID, NULL, (THREADFUNCPTR) &atsc3NdkClientAirwavzRZR::TunerStatusThread, NULL);
        if (sThread_ret != 0) {
            atsc3NdkClientAirwavzRZR::tunerStatusThreadShouldRun = false;
            printf("Status Thread launched unsuccessfully, ret: %d", sThread_ret);
            return 0;
        }
    } else {
        printf("using existing TunerStatusThread, sThread is: %d", (int)tsThreadID);
    }

    atsc3NdkClientAirwavzRZR::atsc3NdkClient_ref->LogMsgF("TuneMultiplePLP: tunerStatus: %ul", tsThreadID);
    return 0;
}

//jjustman-2020-03-11 - handoff to libatsc3 alp parser here on callback invocation
void atsc3NdkClientAirwavzRZR::basebandParserALPCallback(uint32_t plpId, const uint8_t *pPacket, int32_t sPacket, void *pUserData)
{
    atsc3RedZoneParserCallbackData_t *pParserCallbackData = (atsc3RedZoneParserCallbackData_t *)pUserData;

    if(!atsc3NdkClientAirwavzRZR::atsc3NdkClient_ref->atsc3_jni_rx_thread_env) {
        //jjustman-2020-04-21 - hack
        atsc3NdkClientAirwavzRZR::atsc3NdkClient_ref->pinFromRxProcessingThread();
    }

    //printf("alpParserIPv4Callback: pPacket: %p, packetLen: %d,  first 4 bytes 0x%02 0x%02 0x%02 0x%02", pPacket, sPacket, pPacket[0], pPacket[1], pPacket[2], pPacket[3]);

    block_t* alpPayload = block_Duplicate_from_ptr((uint8_t*)pPacket, sPacket);
    block_Rewind(alpPayload);

    atsc3_alp_packet_t* atsc3_alp_packet = atsc3_alp_packet_parse(alpPayload);
    if(atsc3_alp_packet) {
        //alp_completed_packets_parsed++;

        //alp_total_bytes += atsc3_alp_packet->alp_payload->p_size;

        if(atsc3_alp_packet->alp_packet_header.packet_type == 0x00) {

            block_Rewind(atsc3_alp_packet->alp_payload);
            atsc3_phy_mmt_player_bridge_process_packet_phy(atsc3_alp_packet->alp_payload);

        } else if(atsc3_alp_packet->alp_packet_header.packet_type == 0x4) {
            //alp_total_LMTs_recv++;
            atsc3_alp_packet_extract_lmt(atsc3_alp_packet);
        }

        atsc3_alp_packet_free(&atsc3_alp_packet);
    }

    block_Destroy(&alpPayload);

    //printf("alpParserIPv4Callback:: returning");
}

void atsc3NdkClientAirwavzRZR::redZoneCaptureBasebandPacketCallback(RedZoneCaptureBasebandPacket *pPacket, void *pUserData)
{
    atsc3RedZoneParserCallbackData_t *pParserCallbackData = (atsc3RedZoneParserCallbackData_t *)pUserData;

    if (pParserCallbackData->device_mode == RedZoneOperatingModeATSC3)
    {
        RZRBasebandParserParsePacket(hBasebandParser, pPacket);
    }
}

void* atsc3NdkClientAirwavzRZR::TunerStatusThread(void*)
{
    int rval;

    atsc3NdkClientAirwavzRZR::atsc3NdkClient_ref->pinFromRxStatusThread();

    while(atsc3NdkClientAirwavzRZR::tunerStatusThreadShouldRun) {

        usleep(5000000);
        //only actively poll the tuner status if the RF status window is visible
        if(atsc3NdkClientAirwavzRZR::tunerStatusThreadShouldPollTunerStatus) {

            unsigned char lock;
            int SNR, RSSi;
            struct RedZonePLPSpecificInfo3  plp_info;
            struct RedZonePLPStatusInfo     status_info;

            rval = RedZoneCaptureGetProp(hRedZoneCapture, RedZoneMasterLockProp, &lock, sizeof(lock));
            //printf("get Lock %d status %d\n", lock, rval);
            rval = RedZoneCaptureGetProp(hRedZoneCapture, RedZoneSNRProp, &SNR, sizeof(SNR));
            //printf("get SNR %d status %d\n", SNR, rval);
            rval = RedZoneCaptureGetProp(hRedZoneCapture, RedZoneRSSIProp, &RSSi, sizeof(RSSi));
            //printf("get RSSi %d status %d\n", RSSi, rval);
            plp_info.index = 0;         /* Retreive info for first enabled PLP */
            rval = RedZoneCaptureGetProp(hRedZoneCapture, RedZonePLPSpecificInfoProp, &plp_info, sizeof(plp_info));
            rval = RedZoneCaptureGetProp(hRedZoneCapture, RedZonePLPStatusInfoProp, &status_info, sizeof(status_info));

            atsc3NdkClient_ref->atsc3_update_rf_stats(
                    1,                              // tuner lock status
                    RSSi * 10,                      // Tuner RSSi
                    plp_info.mod!=255,              // modcod_valid
                    plp_info.fec_type,              // plp fec type
                    plp_info.mod,                   // plp mod
                    plp_info.cod,                   // plp cod
                    RSSi * 10,                      // RFLevel1000
                    SNR * 10,                       // SNR1000
                    status_info.Pre_LDPC_BER[0],    // ber pre ldpc
                    status_info.Pre_BCH_BER[0],     // ber pre bch
                    status_info.Post_BCH_FER[0],    // fer post
                    lock,                           // demod master lock
                    1,                              // cpu status run - NA for RedZone Receiver
                    plp_info.lls_present,           // plp lls? - name is plp_any so not sure what it is used for
                    0);                             // plp_all - seems to be unused

            // Needs to be connected up to Java callback to display statistics
            //atsc3NdkClientAirwavzRZR->atsc3_update_rf_bw_stats(apiImpl.alp_completed_packets_parsed, apiImpl.alp_total_bytes, apiImpl.alp_total_LMTs_recv);

        }
    }
    return 0;
}

int atsc3NdkClientAirwavzRZR::Close()
{
    printf( "atsc3NdkClientAirwavzRZR Close:  Entered\n");
    return 0;
    // Close seems to cause fault later in the shutdown process - need to debug this
    atsc3NdkClientAirwavzRZR::tunerStatusThreadShouldPollTunerStatus = false;
    atsc3NdkClientAirwavzRZR::tunerStatusThreadShouldRun = false;
    if (tsThreadID)
        //pthread_cancel(tsThreadID);
        //pthread_kill(tsThreadID, 0);
    if (hRedZoneCapture)
    {
        if (fRedZoneCaptureStarted)
        {
            RedZoneCaptureStop(hRedZoneCapture);
        }
        RedZoneCaptureClose(hRedZoneCapture);
        hRedZoneCapture = nullptr;
    }

    if (hBasebandParser)
    {
        RZRBasebandParserClose(hBasebandParser);
        hRedZoneCapture = nullptr;
    }
    printf( "atsc3NdkClientAirwavzRZR Close:  success\n");
}