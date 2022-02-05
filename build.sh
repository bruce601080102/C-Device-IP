#!/bin/bash
echo "build library for" $1 

if [ "$1" == "android" ]; then
    PWD=`pwd`
    NDK=${HOME}/Android/Sdk/ndk/android-ndk-r21e
    BUILDER=${NDK}/ndk-build
    ABS=${PWD}/Andriod.mk
    NAM=${PWD}/Application.mk

    ${BUILDER} NDK_PROJECT_PATH=${PWD} APP_BUILD_SCRIPT=${ABS} NDK_APPLICATION_MK=${NAM}
    rm -rf obj
elif [ "$1" == "ios" ]; then
    mkdir ios
    cp lib.h ./ios/x86_64.h
    gcc -c lib.c -o ./ios/x86_64.o -target x86_64-apple-ios-simulator
    libtool -static ./ios/x86_64.o -o ./ios/x86_64.a 

    cp lib.h ./ios/arm64.h
    gcc -c lib.c -o ./ios/arm64.o -target arm64-apple-ios -fembed-bitcode
    libtool -static ./ios/arm64.o -o ./ios/arm64.a
else
    echo "./build.sh [android/ios]"
fi