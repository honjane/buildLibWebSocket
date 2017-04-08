#!/bin/bash

#
# Build libwebsockets static library for Android
#
# requires debian package xutils-dev for makedepend (openssl make depend)
#

# This is based on http://stackoverflow.com/questions/11929773/compiling-the-latest-openssl-for-android/
# via https://github.com/warmcat/libwebsockets/pull/502

# your NDK path
export NDK=/opt/Android/sdk/ndk-bundle

set -e

# Download packages libz, openssl and libwebsockets

[ ! -f zlib-1.2.8.tar.gz ] && {
wget http://prdownloads.sourceforge.net/libpng/zlib-1.2.8.tar.gz
}

[ ! -f openssl-1.0.2g.tar.gz ] && {
wget https://openssl.org/source/openssl-1.0.2g.tar.gz
}

[ ! -f libwebsockets.tar.gz ] && {
git clone https://github.com/warmcat/libwebsockets.git
tar cvf libwebsockets.tar.gz libwebsockets
}

# create a local android toolchain

export FILE_PATH=
export TOOLCHAIN_PATH=
export TOOL=
export ARCH_FLAGS=
export HEADERS=
export LIBS=
export OPENSSL_ARCH=
export OPENSSL_PARAMS=


function build_ws(){

    # Clean then Unzip
    [ -d zlib-1.2.8 ] && rm -fr zlib-1.2.8
    [ -d openssl-1.0.2g ] && rm -fr openssl-1.0.2g
    [ -d libwebsockets ] && rm -fr libwebsockets
    tar xf zlib-1.2.8.tar.gz
    tar xf openssl-1.0.2g.tar.gz
    tar xzf libwebsockets.tar.gz

    echo " clean success "

    # setup environment to use the gcc/ld from the android toolchain
    export NDK_TOOLCHAIN_BASENAME=${TOOLCHAIN_PATH}/${TOOL}
    export CC=$NDK_TOOLCHAIN_BASENAME-gcc
	export CXX=$NDK_TOOLCHAIN_BASENAME-g++
	export LINK=${CXX}
	export LD=$NDK_TOOLCHAIN_BASENAME-ld
	export AR=$NDK_TOOLCHAIN_BASENAME-ar
	export RANLIB=$NDK_TOOLCHAIN_BASENAME-ranlib
	export STRIP=$NDK_TOOLCHAIN_BASENAME-strip

	# setup buildflags
	export ARCH_LINK=
	export CPPFLAGS=" ${ARCH_FLAGS} -I${HEADERS} -fpic -ffunction-sections -funwind-tables -fstack-protector -fno-strict-aliasing -finline-limit=64 "
	export CXXFLAGS=" ${ARCH_FLAGS} -I${HEADERS} -fpic -ffunction-sections -funwind-tables -fstack-protector -fno-strict-aliasing -finline-limit=64 -frtti -fexceptions "
	export CFLAGS=" ${ARCH_FLAGS} -I${HEADERS} -fpic -ffunction-sections -funwind-tables -fstack-protector -fno-strict-aliasing -finline-limit=64 "
	export LDFLAGS=" ${ARCH_LINK} "

	# configure and build zlib
	[ ! -f ./${FILE_PATH}/lib/libz.a ] && {
	cd zlib-1.2.8
	PATH=$TOOLCHAIN_PATH:$PATH ./configure --static --prefix=$TOOLCHAIN_PATH/..
    mv ./Makefile ./Makefile.old
    sed "s/AR=libtool/AR=`echo ${AR}|sed 's#\/#\\\/#g'`/" ./Makefile.old > Makefile.mid
    sed "s/ARFLAGS=-o/ARFLAGS=-r/" ./Makefile.mid > Makefile
	PATH=$TOOLCHAIN_PATH:$PATH make
	PATH=$TOOLCHAIN_PATH:$PATH make install
	cd ..
	}

	# configure and build openssl
	[ ! -f ./${FILE_PATH}/lib/libssl.a ] && {
	PREFIX=$TOOLCHAIN_PATH/..
	cd openssl-1.0.2g

	./Configure ${OPENSSL_ARCH} --prefix=${PREFIX} ${OPENSSL_PARAMS}
	PATH=$TOOLCHAIN_PATH:$PATH make depend
	PATH=$TOOLCHAIN_PATH:$PATH make
	PATH=$TOOLCHAIN_PATH:$PATH make install_sw
	cd ..
	}

	# configure and build libwebsockets
	[ ! -f ./${FILE_PATH}/lib/libwebsockets.a ] && {
	cd libwebsockets
	[ ! -d build ] && mkdir build
	cd build
	PATH=$TOOLCHAIN_PATH:$PATH cmake \
	  -DCMAKE_C_COMPILER=$CC \
	  -DCMAKE_AR=$AR \
	  -DCMAKE_RANLIB=$RANLIB \
	  -DCMAKE_C_FLAGS="$CFLAGS" \
	  -DCMAKE_INSTALL_PREFIX=$TOOLCHAIN_PATH/.. \
	  -DLWS_WITH_SHARED=OFF \
	  -DLWS_WITH_STATIC=ON \
	  -DLWS_WITHOUT_DAEMONIZE=ON \
	  -DLWS_WITHOUT_TESTAPPS=ON \
	  -DLWS_IPV6=OFF \
	  -DLWS_USE_BUNDLED_ZLIB=OFF \
	  -DLWS_WITH_SSL=ON  \
	  -DLWS_WITH_HTTP2=ON \
	  -DLWS_OPENSSL_LIBRARIES="$TOOLCHAIN_PATH/../lib/libssl.a;$TOOLCHAIN_PATH/../lib/libcrypto.a" \
	  -DLWS_OPENSSL_INCLUDE_DIRS=$TOOLCHAIN_PATH/../include \
	  -DCMAKE_BUILD_TYPE=Release \
	  ..
	PATH=$TOOLCHAIN_PATH:$PATH make
	PATH=$TOOLCHAIN_PATH:$PATH make install
	cd ../..
	}

	echo " build success"
}

#build arm
function build_for_arm(){
    # rm dir
    [ -d android-toolchain-arm ] && rm -fr android-toolchain-arm

	export FILE_PATH=android-toolchain-arm
	export TOOLCHAIN_PATH=`pwd`/${FILE_PATH}/bin
	export TOOL=arm-linux-androideabi
	export ARCH_FLAGS="-mthumb"
	export HEADERS=`pwd`/${TOOLCHAIN_PATH}/sysroot/usr/include
	export LIBS=`pwd`/${TOOLCHAIN_PATH}/sysroot/usr/lib
	export OPENSSL_ARCH="android"
	export OPENSSL_PARAMS="no-shared no-idea no-mdc2 no-rc5 no-zlib no-zlib-dynamic enable-tlsext no-ssl2 no-ssl3 enable-ec enable-ecdh enable-ecp"
	
	$NDK/build/tools/make-standalone-toolchain.sh \
	--platform=android-9 \
	--toolchain=arm-linux-androideabi-4.9 \
	--install-dir=`pwd`/${FILE_PATH}
	build_ws
}

#build x86

function build_for_x86(){
    #rm dir
    [ -d android-toolchain-x86 ] && rm -fr android-toolchain-x86

	export FILE_PATH=android-toolchain-x86
	export TOOLCHAIN_PATH=`pwd`/${FILE_PATH}/bin
	export TOOL=i686-linux-android
	export ARCH_FLAGS="-march=i686 -msse3 -mstackrealign -mfpmath=sse"
	export HEADERS=`pwd`/${TOOLCHAIN_PATH}/sysroot/usr/include
	export LIBS=`pwd`/${TOOLCHAIN_PATH}/sysroot/usr/lib
	export OPENSSL_ARCH="android-x86"
	export OPENSSL_PARAMS="no-asm no-shared no-idea no-mdc2 no-rc5 no-zlib no-zlib-dynamic enable-tlsext no-ssl2 no-ssl3 enable-ec enable-ecdh enable-ecp"

	$NDK/build/tools/make-standalone-toolchain.sh \
	--platform=android-9 \
	--toolchain=x86-4.9 \
	--install-dir=`pwd`/${FILE_PATH}
	build_ws
}

#build x86_64
function build_for_x86_64(){
    #rm dir
    [ -d android-toolchain-x86-64 ] && rm -fr android-toolchain-x86-64

	export FILE_PATH=android-toolchain-x86_64
	export TOOLCHAIN_PATH=`pwd`/${FILE_PATH}/bin
	export TOOL=x86_64-linux-android
	export ARCH_FLAGS=
	export HEADERS=`pwd`/${TOOLCHAIN_PATH}/sysroot/usr/include
	export LIBS=`pwd`/${TOOLCHAIN_PATH}/sysroot/usr/lib
	export OPENSSL_ARCH="linux-x86_64"
	export OPENSSL_PARAMS="no-asm no-shared no-idea no-mdc2 no-rc5 no-zlib no-zlib-dynamic enable-tlsext no-ssl2 no-ssl3 enable-ec enable-ecdh enable-ecp enable-ec_nistp_64_gcc_128"

	$NDK/build/tools/make-standalone-toolchain.sh \
	--platform=android-9 \
	--toolchain=x86_64-4.9 \
	--install-dir=`pwd`/${FILE_PATH}
	build_ws
}
#build based on input
function judge_params(){
	if [ $1 -eq 0 ];
	then echo "Please enter your build target."
	return
	fi
	if [ $1 -gt 1 ];
	then echo "Too many parameters."
	return
	fi
	export input=$(echo $2 | tr '[a-z]' '[A-Z]')
	case ${input} in
	"ARM") echo "***build for arm***"
		build_for_arm
		;;
	"X86") echo "***build for X86***"
		build_for_x86
		;;
	"X86_64") echo "***build for X86***"
		build_for_x86_64
		;;
	"ALL") echo "***build for all architecture***"
		build_for_arm
		build_for_x86
		build_for_x86_64
		;;
	*) echo "Please enter a valid parameter."
		;;
	esac
}
judge_params $# $1