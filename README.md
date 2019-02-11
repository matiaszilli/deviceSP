<h1 align="center">Smart Pool project</h1>

# Thing (device) Section

* [Overview](#overview)
* [Device Parts](#deviceparts)
* [Setting up Raspberry Pi](#settingupraspberrypi)
* [Install Node and Arduino SDK on Raspberry Pi](#installnodeandarduino)
* [Create Thing on AWS IoT Core](#createthingonaws)
* [Communication Arduino - Raspberry Pi](#communicationarduinoraspberry)
* [Arduino Code](#arduinocode)

___

<a name="overview"></a>
## Overview
This document provides step by step instructions to install the Raspberry Pi and connect it to the AWS IoT.  
The device consists of two parts, a Raspberry Pi model 3B+ and an Arduino Uno.

The aws-iot-device-sdk.js is built on top of mqtt.js and provides three classes: 'device', 'thingShadow' and 'jobs'. The 'device' class wraps mqtt.js to provide a secure connection to the AWS IoT platform and expose the mqtt.js interfaces upward. It provides features to simplify handling of intermittent connections, including progressive backoff retries, automatic re-subscription upon connection, and queued offline publishing with configurable drain rate.

<a name="deviceparts"></a>
## Device Parts
The device consists of two parts, on the one hand an Arduino Uno, and on the other a Raspberry Pi model 3B+. 
Both the Raspberry Pi and Arduino Uno are very powerful devices, good at different things. The Arduino boards are awesome at reading inputs and outputs from various different sensors, it has a ATmega328P which is the primary microcontroller and it is very reliable. The Raspberry Pi is basically a mini, open-source Linux computer which allows developers to connect to Internet and AWS IoT to securely work with the message broker. If you put these two together, options are limitless.

<a name="settingupraspberrypi"></a>
## Setting up Raspberry Pi
To get started with Raspberry Pi, you need an operating system. NOOBS (New Out Of Box Software) is an easy operating system install manager for the Raspberry Pi.
To install NOOBS, using a computer with an SD card reader, visit the [Downloads page](http://www.raspberrypi.org/downloads/), click on NOOBS, then click on the Download ZIP button under ‘NOOBS (offline and network install)’, and select a folder to save it to. Then, extract the files from the zip, format your SD card and paste the files on SD card.
Power on your Raspberry Pi, and a window will appear with a list of different operating systems that you can install. We recommend that you use Raspbian – tick the box next to Raspbian and click on Install. When the install process has completed, the Raspberry Pi configuration menu (raspi-config) will load. Here you have to set the time and date for your region and configure a internet connection.

<a name="installnodeandarduino"></a>
## Install Node and Arduino SDK on Raspberry Pi
We will first run the apt “update” command.  This command will not actually update any software on the system, but will download the latest package lists from the software repositories so that Raspbian will be aware of all new software available along with dependencies

    sudo apt update

Next, run the following command to upgrade any packages installed on your system that need upgrades

    sudo apt full-upgrade -y

Then, install node.js and check npm is on the lastest version.

    sudo apt-get install nodejs
    sudo npm install -g npm@latest

To check Node.js is properly installed and you have the right version, run the command node -v and it should return the installed version.

Finally, in order to install run Arduino on your Raspberry, you will need to type the following into the Terminal

    sudo apt-get install arduino

<a name="createthingonaws"></a>
## Create Thing on AWS IoT Core
In the AWS-IoT page, there are lots of instructions, in this page. we will show the fastest way to test connection between Raspberry Pi and AWS-IoT platform. 
Go to Aws Iot Core and select Connect then choose a plataform: Linux/OSX and Select Javascript Device SDK then register a thing and download the certificates and root-CA files. 
The SDK supports three types of connections to the AWS IoT platform:

* MQTT over TLS with mutual certificate authentication using port 8883
* MQTT over WebSocket/TLS with SigV4 authentication using port 443
* MQTT over WebSocket/TLS using a custom authorization function to authenticate

The default connection type is MQTT over TLS with mutual certificate authentication, to set the connection unzip the connect_device_package.zip and transfer certificate files to Raspberry Pi home directory. After above, the /home directory will include below files:

    root-CA.crt
    Test-Device.cert.pem (will be different name for different things)
    Test-Device.private.key (will be different name for different things)
    Test-Device.public.key (will be different name for different things)

Install AWS IoT Node.js SDK via npm at home directory

    npm install aws-iot-device-sdk

Finally copy, edit parameters and execute the script [here](https://github.com/aws/aws-iot-device-sdk-js#device-class).

<a name="communicationarduinoraspberry"></a>
## Communication Arduino - Raspberry Pi
Putting Arduino and Raspberry Pi together allows to separate the computing intensive tasks (done by the Raspberry Pi) and controlling tasks (done by the Arduino). We will connect Arduino board to Raspberry Pi using simple serial communication over USB cable, it is, connect a Raspberry USB port to Arduino USB port.

(ArduinoRaspi.jpg)

To allow Node.js interact with serial port is necessary to install [SerialPort] (https://serialport.io/) library from npm.
In Arduino we use the native functions Serial.available() to check for incoming data and Serial.println() to write data into serial port. 

<a name="arduinocode"></a>
## Arduino Code

The Arduino board allows to interact with different sensors, to do that, some functions have been developed, the most importants are described below:

* `recvInfo()`:
This function runs in a infinit loop and check if data is received from Serial port, in case data is received it executes the corresponding command, as we said the received data are commands to run some task over board.

* `sendInfo()`:
The function read data from temperature, ph and orp sensors and write readed data to Serial port in a JSON format.
 
* `actionPumpOn(int minutes)`:
When this function is triggered, the pump runs for 'minutes' minutes given in the function parameter.

* `sendInfoWPump()`:
This function start the pump for 2 minutes, then stop it, wait a minute and send data (temperature, ph and orp) over Serial using 'sendInfo' function. The goal is flush data to sensor and then poweroff pump before sending data to avoid turbulence and wrong metered data.

* `checkTimers()`:
The function checkTimers check some timers flags and execute actions when timers expires. For example, it checks pump timers, and when it expires, the pump is powered off. The main goal of this strategy is avoid to use Arduino 'delay()' function because that function lock the board and any other action can be run. 
