# Android.mk for libatsc3 + Airwavz RZR
#
#
# jjustman@ngbp.org - for libatsc3 inclusion 2019-09-28
# support@airwavz.tv - for Airwavz RZR inclusion

# global pathing

MY_LOCAL_PATH := $(call my-dir)
LOCAL_PATH := $(call my-dir)
MY_CUR_PATH := $(LOCAL_PATH)

# Include the Airwavz.tv RedZone Receiver SDK Shared Library Modules
include $(LOCAL_PATH)/src/main/jniLibs/rzr-sdk.mk

# ---------------------------
# libatsc3 jni interface

LOCAL_PATH := $(MY_LOCAL_PATH)
include $(CLEAR_VARS)

LOCAL_MODULE := atsc3NdkClient

# Airwavz.tv RedZone Receiver SDK Shared Library Modules
LOCAL_SHARED_LIBRARIES := libredzone_c_api
LOCAL_SHARED_LIBRARIES += libredzone_api
LOCAL_SHARED_LIBRARIES += libRedZoneATSC3Parsers
LOCAL_SHARED_LIBRARIES += libusb

LIBATSC3C := \
    $(wildcard $(LOCAL_PATH)/../../../src/*.c)

LIBATSC3CPP := \
    $(wildcard $(LOCAL_PATH)/../../../src/*.cpp)

LOCAL_SRC_FILES := \
    src/main/jni/atsc3NdkClient.cpp \
    src/main/jni/atsc3NdkClientAirwavzRZR.cpp \
    $(LIBATSC3C:$(LOCAL_PATH)/%=%)  \
    $(LIBATSC3CPP:$(LOCAL_PATH)/%=%)

LOCAL_C_INCLUDES += $(LOCAL_PATH)/src/main/jni
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../../src/
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../Airwavz-RZR-SDK/include

LOCAL_CFLAGS += -g -fpack-struct=8 -fPIC  \
                -D__DISABLE_LIBPCAP__ -D__DISABLE_ISOBMFF_LINKAGE__ -D__DISABLE_NCURSES__ \
                -D__MOCK_PCAP_REPLAY__ -D__LIBATSC3_ANDROID__

LOCAL_LDLIBS := -ldl -llog -landroid -lz

include $(BUILD_SHARED_LIBRARY)

# notes: jjustman-2019-11-26
#
# pack ndk structs for aligned access - http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.faqs/ka15414.html
# https://stackoverflow.com/questions/43559712/android-ndk-c-struct-member-access-causes-sigbus-signal-sigbus-illegal-ali
# cppFlags "-fpack-struct=8"



