# ---------------------------
# RedZone Receiver SDK libredzone_c_api
#
#LOCAL_PATH := $(MY_LOCAL_PATH)
include $(CLEAR_VARS)
LOCAL_MODULE := libredzone_c_api
LOCAL_SRC_FILES := $(LOCAL_PATH)/src/main/jniLibs/$(TARGET_ARCH_ABI)/libredzone_c_api.so
include $(PREBUILT_SHARED_LIBRARY)

# ---------------------------
# RedZone Receiver SDK libredzone_api
#
#LOCAL_PATH := $(MY_LOCAL_PATH)
include $(CLEAR_VARS)
LOCAL_MODULE := libredzone_api
LOCAL_SRC_FILES := $(LOCAL_PATH)/src/main/jniLibs/$(TARGET_ARCH_ABI)/libredzone_api.so
include $(PREBUILT_SHARED_LIBRARY)

# ---------------------------
# RedZone Receiver SDK libRedZoneATSC3Parsers
#
include $(CLEAR_VARS)
LOCAL_MODULE := libRedZoneATSC3Parsers
LOCAL_SRC_FILES := $(LOCAL_PATH)/src/main/jniLibs/$(TARGET_ARCH_ABI)/libRedZoneATSC3Parsers.so
include $(PREBUILT_SHARED_LIBRARY)

# ---------------------------
# RedZone Receiver SDK libusb
#
include $(CLEAR_VARS)
LOCAL_MODULE := libusb
LOCAL_SRC_FILES := $(LOCAL_PATH)/src/main/jniLibs/$(TARGET_ARCH_ABI)/libusb1.0.so
include $(PREBUILT_SHARED_LIBRARY)

