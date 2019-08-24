#!/bin/bash

# The orignal script is based on "https://github.com/markus-perl/ffmpeg-build-script"

VERSION=1.0
CWD=$(pwd)
PACKAGES="$CWD/packages"
WORKSPACE="$CWD/workspace"
LDFLAGS="-L${WORKSPACE}/lib -lm"
CFLAGS="-I${WORKSPACE}/include"
PKG_CONFIG_PATH="${WORKSPACE}/lib/pkgconfig"
ADDITIONAL_CONFIGURE_OPTIONS=""
SUDO_CMD=""
# Speed up the process
# Env Var NUMJOBS overrides automatic detection
if [[ -n $NUMJOBS ]]; then
    MJOBS=$NUMJOBS
elif [[ -f /proc/cpuinfo ]]; then
    MJOBS=$(grep -c processor /proc/cpuinfo)
else
    MJOBS=4
fi

make_dir () {
	if [ ! -d $1 ]; then
		if ! mkdir $1; then
			printf "\n Failed to create dir %s" "$1";
			exit 1
		fi
	fi
}

remove_dir () {
	if [ -d $1 ]; then
		rm -r "$1"
	fi
}

download () {

	DOWNLOAD_PATH=$PACKAGES;
		
	if [ ! -z "$3" ]; then
		mkdir -p $PACKAGES/$3
		DOWNLOAD_PATH=$PACKAGES/$3
	fi;

	if [ ! -f "$DOWNLOAD_PATH/$2" ]; then

		echo "Downloading $1"
		#curl -L --silent -o "$DOWNLOAD_PATH/$2" "$1"
        wget -O "$DOWNLOAD_PATH/$2" "$1"
		EXITCODE=$?
		if [ $EXITCODE -ne 0 ]; then
			echo ""
			echo "Failed to download $1. Exitcode $EXITCODE. Retrying in 10 seconds";
			sleep 10
			#curl -L --silent -o "$DOWNLOAD_PATH/$2" "$1"
			wget -O "$DOWNLOAD_PATH/$2" "$1"
		fi

		EXITCODE=$?
		if [ $EXITCODE -ne 0 ]; then
			echo ""
			echo "Failed to download $1. Exitcode $EXITCODE";
			exit 1
		fi

		echo "... Done"

		if ! tar -xvf "$DOWNLOAD_PATH/$2" -C "$DOWNLOAD_PATH" 2>/dev/null >/dev/null; then
			echo "Failed to extract $2";
			exit 1
		fi

	fi
}

execute () {
	echo "$ $*"

	OUTPUT=$($@ 2>&1)

	if [ $? -ne 0 ]; then
        echo "$OUTPUT"
        echo ""
        echo "Failed to Execute $*" >&2
        exit 1
    fi
}

build () {
	echo ""
	echo "building $1"
	echo "======================="

	if [ -f "$PACKAGES/$1.done" ]; then
		echo "$1 already built. Remove $PACKAGES/$1.done lockfile to rebuild it."
		return 1
	fi

	return 0
}

command_exists() {
    if ! [[ -x $(command -v "$1") ]]; then
        return 1
    fi

    return 0
}

check_sudo(){
	if command_exists "sudo"; then
		SUDO_CMD="sudo"	
	fi
}

build_done () {
	touch "$PACKAGES/$1.done"
}

echo "ffmpeg-libraries-build-script v$VERSION"
echo "========================="
echo ""

case "$1" in
"--cleanup")
	remove_dir $PACKAGES
	remove_dir $WORKSPACE
	echo "Cleanup done."
	echo ""
	exit 0
    ;;
"--build")

    ;;
*)
    echo "Usage: $0"
    echo "   --build: start building process"
    echo "   --cleanup: remove all working dirs"
    echo "   --help: show this help"
    echo ""
    exit 0
    ;;
esac

echo "Using $MJOBS make jobs simultaneously."

make_dir $PACKAGES
make_dir $WORKSPACE

export PATH=${WORKSPACE}/bin:$PATH

if ! command_exists "make"; then
    echo "make not installed.";
    exit 1
fi

if ! command_exists "g++"; then
    echo "g++ not installed.";
    exit 1
fi

#if ! command_exists "curl"; then
if ! command_exists "wget"; then
    echo "wget not installed.";
    exit 1
fi


if build "yasm"; then
	download "http://www.tortall.net/projects/yasm/releases/yasm-1.3.0.tar.gz" "yasm-1.3.0.tar.gz"
	cd $PACKAGES/yasm-1.3.0 || exit
	execute ./configure --prefix=${WORKSPACE}
	execute make -j $MJOBS
	execute make install
	build_done "yasm"
fi

if build "nasm"; then
	download "https://www.nasm.us/pub/nasm/releasebuilds/2.14.02/nasm-2.14.02.tar.gz" "nasm.tar.gz"
	cd $PACKAGES/nasm-2.14.02 || exit
	execute ./configure --prefix=${WORKSPACE} --enable-shared --disable-static
	execute make -j $MJOBS
	execute make install
	build_done "nasm"
fi



if build "pkg-config"; then
	download "http://pkgconfig.freedesktop.org/releases/pkg-config-0.29.2.tar.gz" "pkg-config-0.29.2.tar.gz"
	cd $PACKAGES/pkg-config-0.29.2 || exit
	execute ./configure --silent --prefix=${WORKSPACE} --with-pc-path=${WORKSPACE}/lib/pkgconfig --with-internal-glib
	execute make -j $MJOBS
	execute make install
	build_done "pkg-config"
fi


build "ffmpeg libraries"
download "https://ffmpeg.org/releases/ffmpeg-4.1.tar.bz2" "ffmpeg-snapshot.tar.bz2"
cd $PACKAGES/ffmpeg-4.1 || exit
./configure $ADDITIONAL_CONFIGURE_OPTIONS \
    --pkgconfigdir="$WORKSPACE/lib/pkgconfig" \
    --prefix=${WORKSPACE} \
    --extra-cflags="-I$WORKSPACE/include" \
    --extra-ldflags="-L$WORKSPACE/lib" \
    --extra-libs="-lpthread -lm" \
	--enable-shared \
	--disable-debug \
	--disable-static \
	--disable-ffplay \
	--disable-doc \
	--enable-gpl \
	--enable-version3 \
	--enable-nonfree \
	--enable-pthreads 	

execute make -j $MJOBS
execute make install

    INSTALL_FOLDER="/usr/local"

echo ""
echo "Building done. The libraries can be found here: $WORKSPACE/lib"
echo ""

check_sudo

if [[ $AUTOINSTALL == "yes" ]]; then
	
	${SUDO_CMD} cp -Rf $WORKSPACE/lib/* $INSTALL_FOLDER/lib/
	${SUDO_CMD} cp -Rf $WORKSPACE/include/* $INSTALL_FOLDER/include/
	remove_dir $PACKAGES
	remove_dir $WORKSPACE
	echo "Done. the libraries realted with ffmpeg is now installed to your system"
	
elif [[ ! $SKIPINSTALL == "yes" ]]; then

	read -r -p "Install the libraries to your $INSTALL_FOLDER folder? [Y/n] " response

	case $response in
		[yY][eE][sS]|[yY])
			#echo "$WORKSPACE:$INSTALL_FOLDER"
			${SUDO_CMD} cp -Rf $WORKSPACE/lib/* $INSTALL_FOLDER/lib/
			${SUDO_CMD} cp -Rf $WORKSPACE/include/* $INSTALL_FOLDER/include/
			remove_dir $PACKAGES
			remove_dir $WORKSPACE
			echo "Done. the libraries realted with ffmpeg is now installed to your system"
			;;
	esac	
fi

exit 0
