#!/bin/bash
set -e -o pipefail

eval "$(grep UBUNTU_CODENAME /etc/os-release)"
UBUNTU_ARCH="$(dpkg --print-architecture)"

MONGO_MAJOR_VERSION="6.0"
MONGO_MINOR_VERSION="20"
ORIGINAL_PACKAGE_FILENAME="mongodb-org-mongos_${MONGO_MAJOR_VERSION}.${MONGO_MINOR_VERSION}_${UBUNTU_ARCH}.deb"
ORIGINAL_PACKAGE_URL="https://repo.mongodb.org/apt/ubuntu/dists/${UBUNTU_CODENAME}/mongodb-org/${MONGO_MAJOR_VERSION}/multiverse/binary-${UBUNTU_ARCH}/${ORIGINAL_PACKAGE_FILENAME}"
JOOM_PACKAGE_FILENAME="joom-mongos_${MONGO_MAJOR_VERSION}.${MONGO_MINOR_VERSION}_${UBUNTU_CODENAME}_${UBUNTU_ARCH}.deb"

[ "x$UBUNTU_CODENAME" == "x" -o "x$UBUNTU_ARCH" == "x" ] && { echo "Must run in Ubuntu distro installation, exit... "; exit 1; }

for PKG in build-essential libcurl4-openssl-dev libssl-dev liblzma-dev; do
  FOUND=$(apt -qq list ${PKG} --installed 2>/dev/null| wc -l)
  [ ${FOUND} -ge 1 ] || { echo "Package ${PKG} not found. Install it with 'apt install' command and retry."; exit 1; }
done

WORKDIR="$(pwd)"
SCRIPTDIR="$(dirname $(readlink -f $0))"

# install python prerequisites
python3 -m pip install --upgrade pip
python3 -m pip install -r ${SCRIPTDIR}/../etc/pip/compile-requirements.txt
python3 ${SCRIPTDIR}/../buildscripts/scons.py install-mongos --disable-warnings-as-errors --separate-debug

TMPDIR="$(mktemp -d)"
cd "${TMPDIR}"

# download original deb package
curl -sL "${ORIGINAL_PACKAGE_URL}" -o "${ORIGINAL_PACKAGE_FILENAME}"

# unpack original deb package
ar x "${ORIGINAL_PACKAGE_FILENAME}" && rm -f "${ORIGINAL_PACKAGE_FILENAME}"
tar --zstd -xf control.tar.zst
tar --zstd -xf data.tar.zst
rm control.tar.zst data.tar.zst md5sums

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
