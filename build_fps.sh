#!/bin/bash -xe

JOBS=${JOBS:-16}
TARGET=${TARGET:-"x86_64-native-linuxapp-gcc"}

export ROOT_DIR=$(readlink -e $(dirname $0))


export install_dir=${ROOT_DIR}/out
export dpdk_install_dir=${install_dir}/dpdk
export ofp_install_dir=${install_dir}/ofp
export odp_dpdk_install_dir=${install_dir}/odp

if [ ! -d ${install_dir} ]; then
	mkdir -p ${install_dir}
	echo ">>>>>>>must build odp-dpdk first<<<<<<<<<"
fi

echo '#include "pcap.h"' | cpp -H -o /dev/null 2>&1 || \
    echo "Warning: pcap is not installed. You may need to install libpcap-dev"

echo '#include "numa.h"' | cpp -H -o /dev/null 2>&1 || \
    echo "Warning: NUMA library is not installed. You need to install libnuma-dev"

function build_dpdk()
{
	#git -c advice.detachedHead=false clone -q --depth=1 --branch=17.11 http://dpdk.org/git/dpdk-stable dpdk
	pushd dpdk
	#git log --oneline --decorate

	#Make and edit DPDK configuration
	make config T=${TARGET} O=${dpdk_install_dir}
	pushd ${TARGET}
	#To use I/O without DPDK supported NIC's enable pcap pmd:
	sed -ri 's,(CONFIG_RTE_LIBRTE_PMD_PCAP=).*,\1y,' .config
	popd

	#Build DPDK
	make -j${JOBS} build O=${TARGET} EXTRA_CFLAGS="-fPIC"
	make install O=${TARGET} DESTDIR=${dpdk_install_dir}
	popd
}

function clean_dpdk()
{	
	pushd dpdk/${TARGET}
	make clean
	popd
}

#Build ODP
function build_odp()
{
	# Clone odp-dpdk
	#git clone -q https://github.com/Linaro/odp-dpdk
	pushd odp-dpdk
	#git checkout v1.19.0.2_DPDK_17.11

	export CONFIGURE_FLAGS="--enable-shared=yes --enable-helper-linux"
	
	./bootstrap
	./configure  --enable-debug --enable-debug-print \
			--with-dpdk-path=${dpdk_install_dir}/usr/local/ --prefix=${odp_dpdk_install_dir}/
			
	make -j${JOBS}
	make  install
	popd
}

function clean_odp()
{
	pushd odp-dpdk
	make  clean
	popd
}

function build_ofp()
{
	pushd ofp-master
	
	./bootstrap
	./configure --with-odp=${odp_dpdk_install_dir} --enable-cunit --prefix=${ofp_install_dir}/
	make -j${JOBS}
	make  install
	
	popd
}

function clean_ofp()
{
	pushd ofp-master
	make  clean
	popd
}


for i in "$@"; do
	echo $i
done

BUILD_TARGET="$1"

if [ $BUILD_TARGET == "ofp" ]; then
	build_ofp 
elif  [ $BUILD_TARGET == "odp" ]; then
	build_odp
elif  [ $BUILD_TARGET == "dpdk" ]; then
	build_dpdk
elif  [ $BUILD_TARGET == "all" ]; then
	build_dpdk
	build_odp
	build_ofp 
elif  [ $BUILD_TARGET == "clean" ]; then
	clean_dpdk
	clean_odp
	clean_ofp
	rm -rf ${install_dir}
fi

