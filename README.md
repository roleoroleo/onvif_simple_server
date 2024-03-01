onvif_simple_server_e is a C light implementation of an onvif server intended for use in resource-constrained devices.

So:
- no gsoap
- no libxml


The XML parsing features are replaced by a wrapper that uses ezxml library and a template based strategy.

About security, you can choose between libtomcrypt or mbedtls, to handle authentication.

The web service discovery daemon and the notify server daemon are standalone programs and must be started with command line options.

The onvif server instead runs as CGI and therefore needs an http server that supports the CGI standard (for example httpd from busybox).

## Table of Contents
- [Table of Contents](#table-of-contents)
- [Configuration](#configuration)
- [Compatibility](#compatibility)
- [Credits](#credits)
- [License](#license)
- [Disclaimer](#disclaimer)
- [Donation](#donation)

## Build
- Open the Makefile and edit the path to the libtomcrypt library to suit your needs.
- Run make.

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
user=
password=

#Profile 0
name=Profile_0
width=1920
height=1080
url=rtsp://%s/ch0_0.h264
snapurl=http://%s/cgi-bin/snapshot.sh?res=high&watermark=yes
type=H264

#Profile 1
name=Profile_1
width=640
height=360
url=rtsp://%s/ch0_1.h264
snapurl=http://%s/cgi-bin/snapshot.sh?res=low&watermark=yes
type=H264

#PTZ
ptz=1
get_position=/tmp/sd/yi-hack/bin/ipc_cmd -g
is_moving=/tmp/sd/yi-hack/bin/ipc_cmd -u
move_left=/tmp/sd/yi-hack/bin/ipc_cmd -m left
move_right=/tmp/sd/yi-hack/bin/ipc_cmd -m right
move_up=/tmp/sd/yi-hack/bin/ipc_cmd -m up
move_down=/tmp/sd/yi-hack/bin/ipc_cmd -m down
move_stop=/tmp/sd/yi-hack/bin/ipc_cmd -m stop
move_preset=/tmp/sd/yi-hack/bin/ipc_cmd -p %d
set_preset=/tmp/sd/yi-hack/script/ptz_presets.sh -a add_preset -m %s
set_home_position=/tmp/sd/yi-hack/script/ptz_presets.sh -a set_home_position
remove_preset=/tmp/sd/yi-hack/script/ptz_presets.sh -a del_preset -n %d
jump_to_abs=/tmp/sd/yi-hack/bin/ipc_cmd -j %f,%f
jump_to_rel=/tmp/sd/yi-hack/bin/ipc_cmd -J %f,%f
get_presets=/tmp/sd/yi-hack/script/ptz_presets.sh -a get_presets

#EVENT
events=1
#Event 0
topic=tns1:VideoSource/MotionAlarm
source_name=VideoSourceConfigurationToken
source_value=VideoSourceToken
input_file=/tmp/onvif_notify_server/motion_alarm
#Event 1
topic=tns1:RuleEngine/MyRuleDetector/PeopleDetect
source_name=VideoSourceConfigurationToken
source_value=VideoSourceToken
input_file=/tmp/onvif_notify_server/human_detection
#Event 2
topic=tns1:RuleEngine/MyRuleDetector/VehicleDetect
source_name=VideoSourceConfigurationToken
source_value=VideoSourceToken
input_file=/tmp/onvif_notify_server/vehicle_detection
#Event 3
topic=tns1:RuleEngine/MyRuleDetector/DogCatDetect
source_name=VideoSourceConfigurationToken
source_value=VideoSourceToken
input_file=/tmp/onvif_notify_server/animal_detection
#Event 4
topic=tns1:RuleEngine/MyRuleDetector/BabyCryingDetect
source_name=VideoSourceConfigurationToken
source_value=VideoSourceToken
input_file=/tmp/onvif_notify_server/baby_crying
#Event 5
topic=tns1:AudioAnalytics/Audio/DetectedSound
source_name=VideoSourceConfigurationToken
source_value=VideoSourceToken
input_file=/tmp/onvif_notify_server/sound_detection
```

Note:
- you can use 1 or 2 profiles.
- ipc_cmd is just an example of a local binary that handles ptz, use the specific program of your cam
- %s, %d and %f are placeholders replaced runtime with the proper parameter
- use the same folder for the input files of the events

**Please pay attention: the order of the lines must be respected. Don't mix them!**

Brief explanation of some parameters:

| Param | Description |
| --- | --- |
| ifs | the network interface used by your http server |
| port | the TCP port used by your http server |
| user | the user you want to set for WS-UsernameToken authentication, if blank security is disabled |
| url | the url of your streaming service (it is not provided by onvif server) |
| snapurl | the url of your snapshot service (tipically an http url that provides a jpg image) |
| ptz | 1 if onvif_simple_server can control PTZ, 0 otherwise |
| move_* | the binary that moves the PTZ controls, onvif_simple_server will run it with a system call |
| events | 1 if you want to enable events handling |
| topic | the topic of the event |
| source_name | the source name inside the Notify message |
| source_value | the source value inside the Notify message |
| input_file | the file created when the event is fired |

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
- Onvifer (Android)

If you test it with other clients or NVR, please let me know opening a issue.

Below a list of the implemented functions, all other functions returns a generic soap fault.

**Device**
```
GetServices
GetServiceCapabilities
GetDeviceInformation
GetSystemDateAndTime
SystemReboot
GetScopes
GetUsers
GetWsdlUrl
GetCapabilities
GetNetworkInterfaces
```
**Media**
```
GetServiceCapabilities
GetVideoSources
GetVideoSourceConfigurations
GetVideoSourceConfiguration
GetCompatibleVideoSourceConfigurations
GetVideoSourceConfigurationOptions
GetProfiles
GetProfile
CreateProfile
GetVideoEncoderConfigurations
GetVideoEncoderConfiguration
GetCompatibleVideoEncoderConfigurations
GetVideoEncoderConfigurationOptions
GetGuaranteedNumberOfVideoEncoderInstances
GetSnapshotUri
GetStreamUri
```
**PTZ**
```
GetServiceCapabilities
GetConfigurations
GetConfiguration
GetConfigurationOptions
GetNodes
GetNode
GetPresets
GotoPreset
GotoHomePosition
ContinuousMove
RelativeMove
AbsoluteMove
Stop
GetStatus
SetPreset
SetHomePosition
RemovePreset
```
**Events**
```
GetServiceCapabilities
CreatePullPointSubscription
PullMessages
Subscribe
Renew
Unsubscribe
GetEventProperties
SetSynchronizationPoint
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
