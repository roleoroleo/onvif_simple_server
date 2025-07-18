onvif_simple_server is a C light implementation of an onvif server (Profile S and Profile T) intended for use in resource-constrained devices.

So:
- no gsoap
- no libxml


The XML parsing features are replaced by a wrapper that uses ezxml library and a template based strategy.

About security, you can choose between libtomcrypt, mbedtls or wolfssl to handle authentication.

The web service discovery daemon and the notify server daemon are standalone programs and must be started with command line options.

The onvif server instead runs as CGI and therefore needs an http server that supports the CGI standard (for example httpd from busybox).

Simple also means that it does not implement all the functions required and therefore does not fully comply with the Onvif Conformance Test.
But usually is enough to work with an onvif client.

## Table of Contents
- [Build](#build)
- [Httpd](#httpd)
- [Table of Contents](#table-of-contents)
- [Configuration](#configuration)
- [Compatibility](#compatibility)
- [Credits](#credits)
- [License](#license)
- [Disclaimer](#disclaimer)
- [Donation](#donation)

## Build
- Open the `Makefile` and edit it to suit your needs. Instead of `Makefile` rename `Makefile.static` if you want to build a static binary embedding all libraries (ssl and zlib).
- Export HAVE_WOLFSSL if you want to use wolfssl or HAVE_MBEDTLS if you want to use mbedtls (default libtomcrypt).
- Export USE_ZLIB if you want to use compressed template files. In this case, all xml files will be compressed to use less disk space but the server will be slower.
- Run `make`

## Httpd
### Create a working example with lighttpd
- Open extras folder
- Customize `build.sh` script, if you want
- Run `./build.sh`
- Copy the content of the `_install` folder to `/usr/local`
- Run `/usr/local/bin/lighttpd -f /usr/local/etc/lighttpd.conf`
- Run your preferred client and test the address `http://YOUR_IP:8080/onvif/device_service`

### Use httpd from busybox
If you are using busybox I prepared a patch to use it with onvif_simple_server. The patch is valid for version 1.36.1.
```
diff -Naur busybox-1.36.1.ori/networking/httpd.c busybox-1.36.1/networking/httpd.c
--- busybox-1.36.1.ori/networking/httpd.c	2024-01-08 16:45:49.504118695 +0100
+++ busybox-1.36.1/networking/httpd.c	2024-01-08 16:52:14.701167800 +0100
@@ -2406,6 +2406,13 @@
 		}
 		cgi_type = CGI_NORMAL;
 	}
+	else if (is_prefixed_with(tptr, "onvif/")) {
+		if (tptr[6] == '\0') {
+			/* protect listing "cgi-bin/" */
+			send_headers_and_exit(HTTP_FORBIDDEN);
+		}
+		cgi_type = CGI_NORMAL;
+	}
 #endif
 
 	if (urlp[-1] == '/') {
```

## Configuration
### onvif_simple_server
onvif_simple server supports the following options but you should use them just for debugging purpose

```
Usage: onvif_simple_server [-c CONF_FILE] [-d] [-f]

        -c CONF_FILE, --conf_file CONF_FILE
                path of the configuration file
        -d LEVEL, --debug LEVEL
                enable debug with LEVEL = 0..5 (default 0 = fatal errors)
        -f, --conf_help
                print the help for the configuration file
        -h, --help
                print this help
```
| Option | Description | Example |
| --- | --- | --- |
| conf_file | the path of the file containing the configuration | /etc/onvif_simple_server.conf |
| debug | debug level from 0 to 5 | - |
| conf_help | print the help to create a configuration file | - |

The configuration file is shared between various programs.
Below an example of the configuration file for a cam:
```
model=Yi Hack
manufacturer=Yi
firmware_ver=x.y.z
hardware_id=AFUS
serial_num=AFUSY12ABCDE3F456789
ifs=wlan0
port=80
scope=onvif://www.onvif.org/Profile/Streaming
scope=onvif://www.onvif.org/Profile/T
scope=onvif://www.onvif.org/hardware
scope=onvif://www.onvif.org/name
user=
password=
#Advanced options
adv_enable_media2=0
adv_fault_if_unknown=0
adv_fault_if_set=0
adv_synology_nvr=0

#Profile 0
name=Profile_0
width=1920
height=1080
url=rtsp://%s/ch0_0.h264
snapurl=http://%s/cgi-bin/snapshot.sh?res=high&watermark=yes
type=H264
audio_encoder=AAC
audio_decoder=G711

#Profile 1
name=Profile_1
width=640
height=360
url=rtsp://%s/ch0_1.h264
snapurl=http://%s/cgi-bin/snapshot.sh?res=low&watermark=yes
type=H264
audio_encoder=AAC
audio_decoder=NONE

#PTZ
ptz=1
min_step_x=0
max_step_x=360
min_step_y=0
max_step_y=180
min_step_z=0
max_step_z=0
get_position=/tmp/sd/yi-hack/bin/ipc_cmd -g
is_moving=/tmp/sd/yi-hack/bin/ipc_cmd -u
move_left=/tmp/sd/yi-hack/bin/ipc_cmd -m left -s %f
move_right=/tmp/sd/yi-hack/bin/ipc_cmd -m right -s %f
move_up=/tmp/sd/yi-hack/bin/ipc_cmd -m up -s %f
move_down=/tmp/sd/yi-hack/bin/ipc_cmd -m down -s %f
move_in=/tmp/sd/yi-hack/bin/ipc_cmd -m in -s %f
move_out=/tmp/sd/yi-hack/bin/ipc_cmd -m out -s %f
move_stop=/tmp/sd/yi-hack/bin/ipc_cmd -m stop -t %s
move_preset=/tmp/sd/yi-hack/bin/ipc_cmd -p %d
goto_home_position=/tmp/sd/yi-hack/bin/ipc_cmd -p 0
set_preset=/tmp/sd/yi-hack/script/ptz_presets.sh -a add_preset -n %d -m %s
set_home_position=/tmp/sd/yi-hack/script/ptz_presets.sh -a set_home_position
remove_preset=/tmp/sd/yi-hack/script/ptz_presets.sh -a del_preset -n %d
jump_to_abs=/tmp/sd/yi-hack/bin/ipc_cmd -j %f,%f,%f
jump_to_rel=/tmp/sd/yi-hack/bin/ipc_cmd -J %f,%f,%f
get_presets=/tmp/sd/yi-hack/script/ptz_presets.sh -a get_presets

#RELAY OUTPUTS
#Relay 0
idle_state=open
close=/usr/local/bin/set_relay -n 0 -a close
open=/usr/local/bin/set_relay -n 0 -a open
#Relay 1
idle_state=open
close=/usr/local/bin/set_relay -n 1 -a close
open=/usr/local/bin/set_relay -n 1 -a open

#EVENT
events=1
#Event 0
topic=tns1:VideoSource/MotionAlarm
source_name=Source
source_type=tt:ReferenceToken
source_value=VideoSourceToken
input_file=/tmp/onvif_notify_server/motion_alarm
#Event 1
topic=tns1:RuleEngine/MyRuleDetector/PeopleDetect
source_name=VideoSourceConfigurationToken
source_type=xsd:string
source_value=VideoSourceToken
input_file=/tmp/onvif_notify_server/human_detection
#Event 2
topic=tns1:RuleEngine/MyRuleDetector/VehicleDetect
source_name=VideoSourceConfigurationToken
source_type=xsd:string
source_value=VideoSourceToken
input_file=/tmp/onvif_notify_server/vehicle_detection
#Event 3
topic=tns1:RuleEngine/MyRuleDetector/DogCatDetect
source_name=VideoSourceConfigurationToken
source_type=xsd:string
source_value=VideoSourceToken
input_file=/tmp/onvif_notify_server/animal_detection
#Event 4
topic=tns1:RuleEngine/MyRuleDetector/BabyCryingDetect
source_name=AudioSourceConfigurationToken
source_type=xsd:string
source_value=AudioSourceToken
input_file=/tmp/onvif_notify_server/baby_crying
#Event 5
topic=tns1:AudioAnalytics/Audio/DetectedSound
source_name=AudioSourceConfigurationToken
source_type=tt:ReferenceToken
source_value=AudioSourceToken
input_file=/tmp/onvif_notify_server/sound_detection
```

Note:
- you can use 1 or 2 profiles.
- **Actually only type=h264 is supported on Profile S and both type=h264 and type=h265 are supported in Profile T.** This doesn't mean that if your camera is h265 then it does not work with Profile S. It depends on your onvif client implementation.
- ipc_cmd is just an example of a local binary that handles ptz, use the specific program of your cam

  **If the external command uses stdout, please append a redirect to /dev/null to avoid garbage in the html content**

  example: move_right=/tmp/sd/yi-hack/bin/ipc_cmd -m right > /dev/null

- %s, %d and %f are placeholders replaced runtime with the proper parameter

  move_left=/tmp/sd/yi-hack/bin/ipc_cmd -m left -s %f
  %s is the velocity of the pan movement
  move_right=/tmp/sd/yi-hack/bin/ipc_cmd -m right -s %f
  %s is the velocity of the pan movement
  move_up=/tmp/sd/yi-hack/bin/ipc_cmd -m up -s %f
  %s is the velocity of the tilt movement
  move_down=/tmp/sd/yi-hack/bin/ipc_cmd -m down -s %f
  %s is the velocity of the tilt movement
  move_in=/tmp/sd/yi-hack/bin/ipc_cmd -m in -s %f
  %s is the velocity of the zoom movement
  move_out=/tmp/sd/yi-hack/bin/ipc_cmd -m out -s %f
  %s is the velocity of the zoom movement
  move_stop=/tmp/sd/yi-hack/bin/ipc_cmd -m stop -t %s
  %s is what you want to stop: "pantilt", "zoom" or "all"
  move_preset=/tmp/sd/yi-hack/bin/ipc_cmd -p %d
  %d is the number of the preset you want to move
  set_preset=/tmp/sd/yi-hack/script/ptz_presets.sh -a add_preset -n %d -m %s
  %d is the nunmber of the preset you want to update or -1 if you are adding a preset
  %s is the name of the preset you are adding or changing
  remove_preset=/tmp/sd/yi-hack/script/ptz_presets.sh -a del_preset -n %d
  %d is the nunmber of the preset you want to remove
  jump_to_abs=/tmp/sd/yi-hack/bin/ipc_cmd -j %f,%f,%f
  %f are the pan/tilt/zoom coordinates you want to move
  jump_to_rel=/tmp/sd/yi-hack/bin/ipc_cmd -J %f,%f,%f
  %f are the pan/tilt/zoom coordinates you want to move

- use the same folder for the input files of the events
- max 8 events are currently supported (relay_output feature uses events messages to get the relay state, so each relay uses 1 event and the sum of events and relays cannot be greater than 8)

**Please pay attention: the order of the lines must be respected. Don't mix them!**

Brief explanation of some parameters:

| Param | Description |
| --- | --- |
| ifs | the network interface used by your http server |
| port | the TCP port used by your http server |
| user | the user you want to set for WS-UsernameToken authentication, if blank security is disabled |
| adv_enable_media2 | enable media2 methods for Profile T |
| adv_fault_if_unknown | try to set to 1 if your ONVIF client is not able to connect to the device |
| adv_fault_if_set | try to set to 1 if your ONVIF client is not able to connect to the device |
| adv_synology_nvr | try to set to 1 to improve compatibility, if you are using a Synology NVR |
| url | the url of your streaming service (it is not provided by onvif server) |
| snapurl | the url of your snapshot service (tipically an http url that provides a jpg image) |
| audio_decoder | set to G711 or AAC if your device support an audio back channel |
| ptz | 1 if onvif_simple_server can control PTZ, 0 otherwise |
| max_step_* | max values of x and y movements reported by the cam (min = 0) |
| move_* | the binary command that moves the PTZ controls, onvif_simple_server will run it with a system call |
| idle_state | the relay state when inactive ("close" or "open") |
| close | the binary command that closes the relay, onvif_simple_server will run it with a system call |
| open | the binary command that opens the relay, onvif_simple_server will run it with a system call |
| events | set to 1 to enable PullPoint, 2 to enable Base Subscription or 3 to enable both |
| topic | the topic of the event |
| source_name | the source name inside the Notify message |
| source_name | the source type inside the Notify message |
| source_value | the source value inside the Notify message |
| input_file | the file created when the event is fired |

Check the code for information.

### wsd_simple_server
wsd_simple server supports the following options

```
Usage: wsd_simple_server -i INTERFACE -x XADDR [-m MODEL] [-n MANUFACTURER] -p PID_FILE [-f] [-d LEVEL]

        -i, --if_name
                network interface
        -x, --xaddr
                resource address
        -m, --model
                model name
        -n, --hardware
                hardware manufacturer
        -p, --pid_file
                pid file
        -f, --foreground
                don't daemonize
        -d LEVEL, --debug LEVEL
                enable debug with LEVEL = 0..5 (default 0 = log fatal errors)
        -h, --help
                print this help
```
| Option | Description | Example |
| --- | --- | --- |
| if_name | the network interface that the device binds | eth0 |
| xaddr | the resource address associated to the onvif server |  http://%s/onvif/device_service |
| pid_file | is the pid file created by the daemon | /var/run/wsd_simple_server.pid |
| foreground | don't fork | - |
| debug | debug level from 0 to 5 | - |

%s is replaced runtime with the IP address of the device.

### onvif_notify_server
onvif_notify_server checks the files related to the events and sends the notify message when an event is fired.
- When the file is created a new notify message with state "true" is sent to all subscribers.
- When the file is deleted a new notify message with state "false" is sent to all subscribers.
- When a Synchronization Point is requested from an onvif client, a new Notify message with the current state is sent to all subscribers.

The process can use inotify interface if supported from the device, otherwise it can use the standard "access" function.

onvif_notify_server supports the following options but you should use them just for debugging purpose

```
Usage: onvif_notify_server [-p PID_FILE] [-q NUM] [-f] [-d LEVEL]

        -c CONF_FILE, --conf_file CONF_FILE
                path of the configuration file
        -p PID_FILE, --pid_file PID_FILE
                pid file
        -f, --foreground
                don't daemonize
        -d LEVEL, --debug LEVEL
                enable debug with LEVEL = 0..5 (default 0 = log fatal errors)
        -h, --help
                print this help
```
| Option | Description | Default |
| --- | --- | --- |
| conf_file | the path of the file containing the configuration | /etc/onvif_simple_server.conf |
| pid_file | the path of the pid file if you want to change it | /var/run/onvif_notify_server.pid |
| foreground | don't fork | - |
| debug | debug level from 0 to 5 | 0 |

## Compatibility
I tested this program with the following clients:
- Onvif Device Manager (Windows)
- Synology Surveillance Station (DSM 6.x and 7.x)
- Onvier (Android)
- Frigate
- Unifi Protect

If you test it with other clients or NVR, please let me know opening a issue or a pull request.

Below a list of the implemented functions, all other functions return a generic soap fault.

**Device**
```
GetCapabilities
GetDeviceInformation
GetDiscoveryMode
GetNetworkInterfaces
GetScopes
GetServices
GetServiceCapabilities
GetSystemDateAndTime
GetUsers
GetWsdlUrl
SystemReboot
```

**DeviceIO**
```
GetAudioOutputs
GetAudioSources
GetRelayOutputOptions
GetRelayOutputs
GetServiceCapabilities
GetVideoSources
SetRelayOutputSettings
SetRelayOutputState
```

**Events**
```
CreatePullPointSubscription
GetEventProperties
GetServiceCapabilities
PullMessages
Renew
SetSynchronizationPoint
Subscribe
Unsubscribe
```

**Media**
```
GetAudioDecoderConfiguration
GetAudioDecoderConfigurationOptions
GetAudioDecoderConfigurations
GetAudioEncoderConfiguration
GetAudioEncoderConfigurationOptions
GetAudioEncoderConfigurations
GetAudioOutputConfiguration
GetAudioOutputConfigurationOptions
GetAudioOutputConfigurations
GetAudioOutputs
GetAudioSources
GetAudioSourceConfiguration
GetAudioSourceConfigurationOptions
GetAudioSourceConfigurations
GetCompatibleAudioDecoderConfigurations
GetCompatibleAudioEncoderConfigurations
GetCompatibleAudioOutputConfigurations
GetCompatibleAudioSourceConfigurations
GetCompatibleVideoEncoderConfigurations
GetCompatibleVideoSourceConfigurations
GetGuaranteedNumberOfVideoEncoderInstances
GetProfile
GetProfiles
GetServiceCapabilities
GetSnapshotUri
GetStreamUri
GetVideoSourceConfiguration
GetVideoSourceConfigurationOptions
GetVideoSourceConfigurations
GetVideoEncoderConfiguration
GetVideoEncoderConfigurationOptions
GetVideoEncoderConfigurations
GetVideoSources
```

**Media2**
```
GetAudioDecoderConfigurationOptions
GetAudioDecoderConfigurations
GetAudioEncoderConfigurationOptions
GetAudioEncoderConfigurations
GetAudioOutputConfigurationOptions
GetAudioOutputConfigurations
GetAudioSourceConfigurationOptions
GetAudioSourceConfigurations
GetProfiles
GetServiceCapabilities
GetSnapshotUri
GetStreamUri
GetVideoEncoderConfigurationOptions
GetVideoEncoderConfigurations
GetVideoSourceConfigurationOptions
GetVideoSourceConfigurations
GetVideoSourceModes
```

**PTZ**
```
AbsoluteMove
ContinuousMove
GetConfiguration
GetConfigurationOptions
GetConfigurations
GetNode
GetNodes
GetPresets
GetServiceCapabilities
GetStatus
GotoHomePosition
GotoPreset
RelativeMove
RemovePreset
SetHomePosition
SetPreset
Stop
```

## Credits
Thanks to:
- rxi - for the logging library https://github.com/rxi/log.c
- Aaron Voisine - for ezxml library https://ezxml.sourceforge.net/

## License
[GPLv3](https://choosealicense.com/licenses/gpl-3.0/)

## DISCLAIMER
**NOBODY BUT YOU IS RESPONSIBLE FOR ANY USE OR DAMAGE THIS SOFTWARE MAY CAUSE. THIS IS INTENDED FOR EDUCATIONAL PURPOSES ONLY. USE AT YOUR OWN RISK.**

## Donation
If you like this project, you can buy me a beer :) 

Click [here](https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=JBYXDMR24FW7U&currency_code=EUR&source=url) or use the below QR code to donate via PayPal
<p align="center">
  <img src="https://github.com/roleoroleo/onvif_simple_server/assets/39277388/9cccaf46-3575-48ad-a8f4-8b3a5e4c9948"/>
</p>
