<p align="center">
	<a target="_blank" href="https://github.com/roleoroleo/onvif_simple_server/releases">
		<img src="https://img.shields.io/github/downloads/roleoroleo/onvif_simple_server/total.svg" alt="Releases Downloads">
	</a>
</p>

onvif_simple_server is a light implementation of an onvif server intended for use in resource-constrained devices.

So:
- no gsoap
- no libxml


The XML parsing features are replaced by some trivial functions and a template based strategy.

The only library used is libtomcrypt, to handle authentication.

The web service discovery daemon is a standalone program and must be started with command line options.

The onvif server instead runs as CGI and therefore needs an http server that supports the CGI standard. I use httpd from busybox.

## Table of Contents
- [Table of Contents](#table-of-contents)
- [Configuration](#configuration)
- [License](#license)
- [Disclaimer](#disclaimer)
- [Donation](#donation)

## Build
Open the Makefile and edit the path to the libtomcrypt library to suit your needs.
Run make.

## Configuration
### wsd_simple_server
wsd_simple server supports the following options

```
Usage: wsd_simple_server -i INTERFACE -x XADDR -p PID_FILE [-f] [-d LEVEL]

        -i, --if_name
                network interface
        -x, --xaddr
                resource address
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

### onvif_simple_server
onvif_simple server supports the following options

```
Usage: /tmp/sd/yi-hack/www/onvif/onvif_simple_server [-c CONF_FILE] [-d] [-f]

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

Below an example of a configuration file for a cam:
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
move_left=/tmp/sd/yi-hack/bin/ipc_cmd -M left
move_right=/tmp/sd/yi-hack/bin/ipc_cmd -M right
move_up=/tmp/sd/yi-hack/bin/ipc_cmd -m up
move_down=/tmp/sd/yi-hack/bin/ipc_cmd -m down
move_stop=/tmp/sd/yi-hack/bin/ipc_cmd -M stop
move_preset=/tmp/sd/yi-hack/bin/ipc_cmd -p %t
```

You can use 1 or 2 profiles.

Please pay attention: the order of the lines must be respected. Don't mix them!

Brief explanation of some parameters:

| Param | Description |
| --- | --- |
| ifs | the network interface used by your http server |
| port | the TCP port used by your http server |
| user | the user you want to set for WS-UsernameToken authentication, if blank security is disabled |
| url | the url of your streaming service (it is not provided by onvif server) |
| snapurl | the url of your snapshot service (tipically an http url that provides a jpg image) |
| ptz | 1 if onvif_simple_server can control PTZ, 0 otherwise |
| move_* | the binary that move the PTZ controls, this daemon will run it with a system call |

----

## License
[MIT](https://choosealicense.com/licenses/mit/)

## DISCLAIMER
**NOBODY BUT YOU IS RESPONSIBLE FOR ANY USE OR DAMAGE THIS SOFTWARE MAY CAUSE. THIS IS INTENDED FOR EDUCATIONAL PURPOSES ONLY. USE AT YOUR OWN RISK.**

## Donation
If you like this project, you can buy roleo a beer :) 
[![paypal](https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif)](https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=JBYXDMR24FW7U&currency_code=EUR&source=url)
