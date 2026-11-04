# pc/android/app/jni/src/Android.mk  (patch 30.1)
# Builds libmain.so = the real game. It reads the two files that `PC_ANDROID_PREP=1 bash pc/build.sh` writes, so the APK and build.sh never drift apart:
#   pc/gen/sources.txt  every .c file, relative to pc/
#   pc/gen/cflags.txt   the compiler flags build.sh uses (absolute include paths)
# (the old gba_test.c address probe is no longer built; the file is still in this folder)
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := main

SDL_PATH := ../SDL
PC_REL   := ../../../..
PC_DIR   := $(LOCAL_PATH)/$(PC_REL)

PC_SOURCES := $(strip $(shell cat $(PC_DIR)/gen/sources.txt 2>/dev/null))
PC_FLAGS   := $(strip $(shell cat $(PC_DIR)/gen/cflags.txt 2>/dev/null))
ifeq ($(PC_SOURCES),)
$(error pc/gen/sources.txt is missing or empty: run "PC_ANDROID_PREP=1 bash pc/build.sh" first)
endif
ifeq ($(PC_FLAGS),)
$(error pc/gen/cflags.txt is missing or empty: run "PC_ANDROID_PREP=1 bash pc/build.sh" first)
endif

# SDL2's real headers (from the SDL clone), not pc/platform/sdl2/include; -fPIE is dropped (a shared library gets -fPIC from the NDK)
LOCAL_CFLAGS := $(filter-out -I%/platform/sdl2/include -fPIE,$(PC_FLAGS))
LOCAL_C_INCLUDES := $(LOCAL_PATH)/$(SDL_PATH)/include

# paths relative to this folder, so ndk-build names the object files itself
LOCAL_SRC_FILES := $(addprefix $(PC_REL)/,$(PC_SOURCES))

LOCAL_SHARED_LIBRARIES := SDL2

LOCAL_LDLIBS := -lGLESv1_CM -lGLESv2 -lOpenSLES -llog -landroid -lm
LOCAL_LDFLAGS := -Wl,-z,max-page-size=16384

include $(BUILD_SHARED_LIBRARY)
