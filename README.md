Actisense plug-in for OpenCPN
==========================

The Actisense® plugin for OpenCPN integrates OpenCPN with NMEA2000® networks that are connected with an Actisense NGT-1 adapter. 

It also allows users to replay voyages using Actisense EBL log files that have previously been created with the NGT-1 adapter.

This plugin is based on the TwoCan plugin, however as I do not own an Actisense NGT-1 adapter some features present in the TwoCan plugin (Active mode, Logging, Network Map) have been disabled as I have no way to understand how to implement nor test these features in the Actisense plugin.

Please note that this plugin has been developed independently and users should not assume that Active Research Limited approves the use of this plugin with their devices nor provides any level of support.

List of supported NMEA 2000 Parameter Group Numbers (PGN)<sup>1</sup>
---------------------------------------------------------

|PGN|Description|
|---|-----------|
|59392| ISO Acknowledgement|
|59904| ISO Request|
|60928| ISO Address Claim|
|65240| ISO Commanded Address|
|126992| NMEA System Time|
|126993| NMEA Heartbeat|
|126996| NMEA Product Information|
|127245| NMEA Rudder|
|127250| NMEA Vessel Heading|
|127251| NMEA Rate of Turn|
|127257| NMEA Attitude|
|127258| NMEA Magnetic Variation|
|127488| NMEA Engine Parameters, Rapid Update|
|127489| NMEA Engine Paramters, Dynamic|
|127505| NMEA Fluid Levels|
|127508| NMEA Battery Status|
|128259| NMEA Speed & Heading|
|128267| NMEA Depth|
|128275| NMEA Distance Log|
|129025| NMEA Position Rapid Update|
|129026| NMEA COG SOG Rapid Update|
|129029| NMEA GNSS Position|
|129033| NMEA Date & Time|
|129038| AIS Class A Position Report|
|129039| AIS Class B Position Report
|129040| AIS Class B Extended Position Report|
|129041| AIS Aids To Navigation (AToN) Report|
|129283| NMEA Cross Track Error|
|129284| NMEA Navigation Data|
|129285| NMEA Navigation Route/Waypoint Information|
|129793| AIS Date and Time Report|
|129794| AIS Class A Static Data|
|129798| AIS SAR Aircraft Position Report|
|129801| AIS Addressed Safety Related Message|
|129802| AIS Safety Related Broadcast Message|
|129808| NMEA DSC Message|
|129809| AIS Class B Static Data Report, Part A|
|129810| AIS Class B Static Data Report, Part B|
|130306| NMEA Wind|
|130310| NMEA Water & Air Temperature and Pressure|
|130311| NMEA Environmental Parameters (supercedes 130310)|
|130312| NMEA Temperature|
|130316| NMEA Temperature Extended Range|
|130577| NMEA Direction Data|

1. Some of these PGN's may never be sent to the plugin as they may be used internally by the NGT-1.

Obtaining the source code
-------------------------

git clone https://github.com/twocanplugin/actisense.git

Build Environment
-----------------

Both Windows and Linux are currently supported.

This plugin builds outside of the OpenCPN source tree

For both Windows and Linux, refer to the OpenCPN developer manual for details regarding other requirements such as git, cmake and wxWidgets.

For Windows you must place opencpn.lib into the twocan_pi/build directory to be able to link the plugin DLL. opencpn.lib can be obtained from your local OpenCPN build, or alternatively downloaded from http://sourceforge.net/projects/opencpnplugins/files/opencpn_lib/

Build Commands
--------------
 mkdir actisense_pi/build

 cd actisense_pi/build

 cmake ..

 cmake --build . --config debug

  or

 cmake --build . --config release

Creating an installation package
--------------------------------
 cmake --build . --config release --target package

  or

 cpack

Installation
------------
Run the resulting setup package created above for your platform.

Eg. For Windows run actisense\_pi\_1.0.0-ov50-win32.exe

Eg. For Ubuntu (on PC's) run sudo dpkg -i actisense\_pi\_1.0.0-1_amd64.deb

Eg. For Raspberry Pi run sudo dpkg -i actisense\_pi\_1.0.0-1_armhf.deb

Problems
--------
I expect there will be many due to my inability to test this plugin !

If building using gcc, note that are many -Wwrite-strings, -Wunused-but-set-variable and -Wunused-variable warnings, that I'll get around to fixing one day, but at present can be safely ignored.

Please send bug reports/questions/comments to the opencpn forum or via email to twocanplugin@hotmail.com

License
-------
The plugin code is licensed under the terms of the GPL v3 or, at your convenience, a later version.

Actisense® is a registered trademark of Active Research Limited.
NMEA2000® is a registered trademark of the National Marine Electronics Association.
