LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := gojni
# LOCAL_SRC_FILES := ${PWD}/include/libdiia.c
# LOCAL_SRC_FILES := ${PWD}/include/intranet.c

# LOCAL_SRC_FILES := ${PWD}/include_copy/cpu_name.c
# LOCAL_SRC_FILES := ${PWD}/include_copy/mac_address.c
# LOCAL_SRC_FILES := ${PWD}/include/libmacaddress.c

MY_CPP_LIST := $(wildcard ${PWD}/include/*.c)

LOCAL_SRC_FILES  := $(MY_CPP_LIST:%=%)

include $(BUILD_SHARED_LIBRARY)