#!/bin/bash

# Use this script to build a working example of onvif_simple_server
# This script will compile lighttpd, tls library and the server
# and will prepare a folder with all the files you need.

INSTALL_DIR="_install"
export HAVE_MBEDTLS=1
LIGHTTPD=lighttpd-1.4.73
CUR_DIR=$(pwd)

# Don't edit below this line

cd ..
patch -p1 < $CUR_DIR/path.patch
cd extras

mkdir -p $INSTALL_DIR/bin
mkdir -p $INSTALL_DIR/etc/onvif_notify_server
mkdir -p $INSTALL_DIR/etc/wsd_simple_server
mkdir -p $INSTALL_DIR/lib
mkdir -p $INSTALL_DIR/www/onvif

### LIGHTTPD ###
if [ ! -f lighttpd-1.4.73.tar.gz ]; then
    wget https://download.lighttpd.net/lighttpd/releases-1.4.x/$LIGHTTPD.tar.gz
fi
tar zxvf $LIGHTTPD.tar.gz
cd $LIGHTTPD

# Run configure with minimal options
./configure --disable-ipv6 --without-pcre2 --without-zlib

make
cd ..

# Don't use 'make install'
# Copy only essential files
cp $LIGHTTPD/src/lighttpd $INSTALL_DIR/bin
cp $LIGHTTPD/src/.libs/mod_cgi.so $INSTALL_DIR/lib

# Strip binaries
if [ ! -z $STRIP ]; then
    $STRIP $INSTALL_DIR/bin/lighttpd
    $STRIP $INSTALL_DIR/lib/mod_cgi.so
fi

# Create trivial configuration file
echo "server.document-root = \"/usr/local/www/\"" > $INSTALL_DIR/etc/lighttpd.conf
echo "" >> $INSTALL_DIR/etc/lighttpd.conf
echo "server.port = 8080" >> $INSTALL_DIR/etc/lighttpd.conf
echo "" >> $INSTALL_DIR/etc/lighttpd.conf
echo "server.modules = ( \"mod_cgi\" )" >> $INSTALL_DIR/etc/lighttpd.conf
echo "cgi.assign = ( \"_service\" => \"\" )" >> $INSTALL_DIR/etc/lighttpd.conf

### ONVIF_SIMPLE_SERVER ###
cd ..
make
cd extras

cp ../onvif_simple_server $INSTALL_DIR/www/onvif
ln -s ./onvif_simple_server $INSTALL_DIR/www/onvif/device_service
ln -s ./onvif_simple_server $INSTALL_DIR/www/onvif/events_service
ln -s ./onvif_simple_server $INSTALL_DIR/www/onvif/media_service
ln -s ./onvif_simple_server $INSTALL_DIR/www/onvif/media2_service
ln -s ./onvif_simple_server $INSTALL_DIR/www/onvif/ptz_service
ln -s ./onvif_simple_server $INSTALL_DIR/www/onvif/deviceio_service
cp -R ../device_service_files $INSTALL_DIR/www/onvif
cp -R ../events_service_files $INSTALL_DIR/www/onvif
cp -R ../generic_files $INSTALL_DIR/www/onvif
cp -R ../media_service_files $INSTALL_DIR/www/onvif
cp -R ../media2_service_files $INSTALL_DIR/www/onvif
cp -R ../ptz_service_files $INSTALL_DIR/www/onvif
cp -R ../deviceio_service_files $INSTALL_DIR/www/onvif

cp ../onvif_notify_server $INSTALL_DIR/bin || exit 1
cp -R ../notify_files/* $INSTALL_DIR/etc/onvif_notify_server

cp ../wsd_simple_server $INSTALL_DIR/bin || exit 1
cp -R ../wsd_files/* $INSTALL_DIR/etc/wsd_simple_server

cp ../onvif_simple_server.conf.example $INSTALL_DIR/etc/onvif_simple_server.conf

# Strip binaries
if [ ! -z $STRIP ]; then
    ${STRIP} $INSTALL_DIR/bin/onvif_notify_server || exit 1
    ${STRIP} $INSTALL_DIR/bin/wsd_simple_server || exit 1
    ${STRIP} $INSTALL_DIR/www/onvif/onvif_simple_server || exit 1
fi
