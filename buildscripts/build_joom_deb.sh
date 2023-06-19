#!/bin/bash
set -e -o pipefail

eval "$(grep UBUNTU_CODENAME /etc/os-release)"
UBUNTU_ARCH="$(dpkg --print-architecture)"

MONGO_MAJOR_VERSION="5.0"
MONGO_MINOR_VERSION="16"
JOOM_PACKAGE_FILENAME="joom-mongos_${MONGO_MAJOR_VERSION}.${MONGO_MINOR_VERSION}_${UBUNTU_CODENAME}_${UBUNTU_ARCH}.deb"
ORIGINAL_PACKAGE_FILENAME="mongodb-org-mongos_${MONGO_MAJOR_VERSION}.${MONGO_MINOR_VERSION}_${UBUNTU_ARCH}.deb"
ORIGINAL_PACKAGE_URL="https://repo.mongodb.org/apt/ubuntu/dists/${UBUNTU_CODENAME}/mongodb-org/${MONGO_MAJOR_VERSION}/multiverse/binary-${UBUNTU_ARCH}/${ORIGINAL_PACKAGE_FILENAME}"

[ "x$UBUNTU_CODENAME" == "x" -o "x$UBUNTU_ARCH" == "x" ] && { echo "Must run in Ubuntu distro installation, exit... "; exit 1; }

WORKDIR="$(pwd)"
SCRIPTDIR="$(dirname $(readlink -f $0))"
TMPDIR="$(mktemp -d)"

# ubuntu 18.04 bionic has python 3.6 by default, but we need at
# least 3.7 to build mongo. Here are instructions to install python3.8:
# sudo apt update && sudo apt install software-properties-common
# sudo add-apt-repository ppa:deadsnakes/ppa
# sudo apt install python3.8 python3.8-venv python3.8-dev
# python3.8 -m venv /tmp/venv && source /tmp/venv/bin/activate

# ubuntu 18.04 bionic has gcc7.5 by default, bu we need at least 8.2
# to build mongo. Here are instructions to install gcc8:
# sudo apt install g++-8 gcc-8 libc6-dev libc-dev
# sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-8 800 --slave /usr/bin/g++ g++ /usr/bin/g++-8

# cd "${SCRIPTDIR}../"
# apt install build-essential libcurl4-openssl-dev libssl-dev liblzma-dev
# python3 -m pip install --upgrade pip
# python3 -m pip install -r etc/pip/compile-requirements.txt
# python3 buildscripts/scons.py install-mongos --disable-warnings-as-errors --separate-debug

cd "${TMPDIR}"

# download original deb package
curl -sL "${ORIGINAL_PACKAGE_URL}" -o "${ORIGINAL_PACKAGE_FILENAME}"

# unpack original deb package
ar x "${ORIGINAL_PACKAGE_FILENAME}" && rm -f "${ORIGINAL_PACKAGE_FILENAME}"
xz -d control.tar.xz data.tar.xz
tar -xf control.tar && tar -xf data.tar && rm control.tar data.tar md5sums

# replace metadata and binary
sed -i -n '/Installed-Size/!p' control && sed -i 's/\(Package:\).*/\1 joom-mongos/; s/\(Source:\).*/\1 Joom/; s/\(Maintainer: \).*/\1 Joom Storages Infra/' control
cp "${SCRIPTDIR}/../build/install/bin/mongos" usr/bin/mongos

# pack deb archive
tar -czf data.tar.gz usr
tar -czf control.tar.gz control
ar -r "${JOOM_PACKAGE_FILENAME}" debian-binary control.tar.gz data.tar.gz 2>/dev/null 1>&2

# move it to install dir
cp "${JOOM_PACKAGE_FILENAME}" "${SCRIPTDIR}/../build/install/"
echo "${SCRIPTDIR}/../build/install/${JOOM_PACKAGE_FILENAME}"

# cleanup
cd "${WORKDIR}"
rm -rf "${TMPDIR}"
