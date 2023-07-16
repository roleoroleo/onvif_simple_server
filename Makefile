# Set HAVE_MBEDTLS variable if you want to use MBEDTLS instead of TOMCRYPT

OBJECTS_O = onvif_simple_server.o device_service.o media_service.o ptz_service.o utils.o log.o
OBJECTS_W = wsd_simple_server.o utils.o log.o
ifdef HAVE_MBEDTLS
INCLUDE = -DHAVE_MBEDTLS -I../mbedtls/include -ffunction-sections -fdata-sections
LIBS_O = -Wl,--gc-sections ../mbedtls/library/libmbedcrypto.a -lpthread
else
INCLUDE = -I../libtomcrypt/src/headers -ffunction-sections -fdata-sections
LIBS_O = -Wl,--gc-sections ../libtomcrypt/libtomcrypt.a -lpthread
endif
LIBS_W = -Wl,--gc-sections

all: onvif_simple_server wsd_simple_server

onvif_simple_server.o: onvif_simple_server.c $(HEADERS)
	$(CC) -c $< -fPIC -Os $(INCLUDE) -o $@

wsd_simple_server.o: wsd_simple_server.c $(HEADERS)
	$(CC) -c $< -fPIC -Os $(INCLUDE) -o $@

device_service.o: device_service.c $(HEADERS)
	$(CC) -c $< -fPIC -Os $(INCLUDE) -o $@

media_service.o: media_service.c $(HEADERS)
	$(CC) -c $< -fPIC -Os $(INCLUDE) -o $@

ptz_service.o: ptz_service.c $(HEADERS)
	$(CC) -c $< -fPIC -Os $(INCLUDE) -o $@

utils.o: utils.c $(HEADERS)
	$(CC) -c $< -fPIC -Os $(INCLUDE) -o $@

log.o: log.c $(HEADERS)
	$(CC) -c $< -fPIC -Os -std=c99 $(INCLUDE) -o $@

onvif_simple_server: $(OBJECTS_O)
	$(CC) $(OBJECTS_O) $(LIBS_O) -fPIC -Os -o $@
	$(STRIP) $@

wsd_simple_server: $(OBJECTS_W)
	$(CC) $(OBJECTS_W) $(LIBS_W) -fPIC -Os -o $@
	$(STRIP) $@

.PHONY: clean

clean:
	rm -f onvif_simple_server
	rm -f wsd_simple_server
	rm -f $(OBJECTS_O)
	rm -f $(OBJECTS_W)
