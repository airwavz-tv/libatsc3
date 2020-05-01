AIRWAVZ-README 4/30/2020

To demonstrate the correct usage of the Airwavz RedZone Receiver SDK on
non-rooted Android devices we have added some basic support for the
Airwavz.tv RedZone Receiver to the open source libatsc3 project.

The Airwavz.tv version of this project can be downloaded from GitHub at:

git@github.com:airwavz-tv/libatsc3.git

Please download or clone this github project and follow these steps to build
and test it on Android. The changes Airwavz.tv RedZone Receiver support
is part of the airwavz.tv-redzone-receuver branch. Comparing this branch to
master will show all the changes required to support the device.

To build this project you must first place a copy of the files from the include
directory of the Airwavz RedZone Receive SDK into the following directory:

.../sample_app_no_phy/Airwavz-RZR-SDK

Place a copy of the contents of the lib/android directory from the SDK into:

    .../sample_app_no_phy/app/src/main/jniLibs/
    
Copy both the armeabi-v7a folder and the arm64-v8a folder if you want support
for both 32 and 64 bit.

Once this is complete you should be able to import the project into Android
Studio and build it. Our tests have been conducted using the latest Android
Studio and NDK r21.

Our testing has been conducted on an nVidia Shield Pro 500GB (2017) with the
latest release based on Android Pie. The SDK is built to support API Level 24
and beyond.

When you run the app on Android it should ask for permission to access the
RedZone Receiver device. Once permission is granted the device should be listed
and clicking open should open the device and initialize the SDK.

Note that the RZR-1400 will initialize the very first time this is attempted,
however the RZR-1200, which requires firmware to be downloaded, will need open
performed twice. On the first selection of open the RZR-1200 will have the
firmware download performed at which time it will re-ennumerate.

On the second and subsequent open the RZR-1200 will open and operate normally
until the next time it is powered off.

Once the device is open the "Tune" button should be available to tune the device.
The default frequency in the Andoird UI is currently 647MHz.

Once tuned the "RF Stats" button will show the current Lock, RSSi, and SNR.
You should see the SLS and be able to pick and then Play a service.









