# Nabto WebRTC SDK C++

C++ device implementation of Nabto WebRTC Signaling. Since this only implements
the device side, the demo application must be used with a client from another
SDK (iOS, Android, JavaScript).


## Usage

The Libdatachannel demo runs on macOS and Linux.

```
git submodule update --init --recursive
cd examples/libdatachannel
cmake --workflow --preset debug
```

After building, test if the executables exist:

```
./build/debug/install/bin/webrtc_device --help
./build/debug/install/bin/webrtc_device_rtsp --help
```

Generate a key pair:

Go to the [Nabto Cloud Console](https://console.cloud.nabto.com/) and create a
WebRTC product and device. On the device page, generate a key pair, download the
key files, and add the public key to the device.


The demos requires a video feed to stream to the client. For testing the RTP
demo, a feed can be started using a gstreamer testvideosrc UDP RTP feed:

```
gst-launch-1.0 videotestsrc ! clockoverlay ! video/x-raw,width=1920,height=1200 ! \
    videoconvert ! queue !   x264enc tune=zerolatency bitrate=1000 key-int-max=30 ! \
    video/x-h264, profile=constrained-baseline !   rtph264pay pt=96 mtu=1200 ! \
    udpsink host=127.0.0.1 port=6000
```

The following command can send a feed with test audio as well.
```
gst-launch-1.0 videotestsrc ! videoconvert ! x264enc tune=zerolatency bitrate=2000 speed-preset=ultrafast ! \
    rtph264pay pt=96 ! udpsink host=127.0.0.1 port=6000 \
    audiotestsrc ! audioconvert ! audioresample ! opusenc ! \
    rtpopuspay pt=111 ! udpsink host=127.0.0.1 port=6002
```

To receive audio/video in a two-way audio video scenario you can use the following gstreamer command to consume an incoming stream
```
gst-launch-1.0 udpsrc port=6001 caps=application/x-rtp,encoding-name=H264,payload=96 ! rtph264depay ! 
    avdec_h264 ! videoconvert ! autovideosink \
    udpsrc port=6003 caps=application/x-rtp,encoding-name=OPUS,payload=111 ! rtpopusdepay ! \
    opusdec !   audioconvert !   autoaudiosink
```

For testing RTSP, Nabto provides a test RTSP server also based on Gstreamer
[here](https://github.com/nabto/edge-device-webrtc/tree/main/test-apps/rtsp-server).

See our [simulated video sources
guide](https://docs.nabto.com/developer/guides/video/simulated-video-sources.html)
for more options for simulating a video feed.

Run the RTP device:

```
./build/debug/install/bin/webrtc_device -d <deviceId> -p <productId> \
   -k key.pem --secret <secret>
```

Run the RTSP device:

```
./build/debug/install/bin/webrtc_device_rtsp -d <deviceId> -p <productId> \
   -k key.pem --secret <secret> \
   -r rtsp://127.0.0.1:8554/video
```

## Limitations

The libdatachannel WebRTC library used by the examples does not currently support offers generated with `restartIce()` due to this [issue](https://github.com/paullouisageneau/libdatachannel/issues/545). This means if one of the peers experiences a network failure or switches to a different network, any open WebRTC connections cannot be renegotiated. Instead the connection has to be recreated from scratch.

## Cross-build for embedded systems

The build system is based on `vcpkg`. This is an open-source package manager designed to simplify managing C/C++ libraries on various platforms. It simplifies downloading, building and integrating dependencies into projects, reducing the complexity of managing libraries manually and ensuring consistency across different environments. [Read more about vcpkg](https://vcpkg.io/en/).

`vcpkg` uses [_triplets_](https://learn.microsoft.com/en-us/vcpkg/concepts/triplets) to define the target architecture, platform and library linkage which is crucial for cross-compiling dependencies for embedded systems. A triplet looks like `arm64-linux-dynamic`. The linkage configuration is often omitted and a default is used based on the specified architecture and platform. So to cross compile for e.g. an ARM 64-bit based Linux system, you often just specify `arm64-linux`.

### Cross-compilation approaches

There are three levels of cross-compilation support, depending on your target architecture:

| Support level | Description | Example | Approach |
|---------------|-------------|---------|----------|
| **Project preset** | This project includes a preset | ARM64 | Use `cmake --workflow --preset` |
| **vcpkg triplet** | vcpkg supports the triplet, but no project preset exists | ARM32, MIPS64 | Manual cmake with existing triplet |
| **Unsupported** | vcpkg has no triplet for this architecture | MIPS32 | Create custom triplet + manual cmake |

The following sections cover each approach.

### 1. Using a project preset

The project includes presets for common cross-compilation targets. First ensure submodules are initialized (see [Usage](#usage)), then use the preset:

**ARM 64-bit Linux:**
```
cd examples/libdatachannel
cmake --workflow --preset linux_arm64_crosscompile
```

The binaries will be installed to `build/linux_arm64_crosscompile/install/bin/`.

### 2. Using an existing vcpkg triplet (no project preset)

For architectures that vcpkg supports but this project has no preset for, you can configure the build manually. You must:

1. Set the `CC` and `CXX` environment variables to point to your cross-compiler
2. Pass the appropriate vcpkg triplet and CMake system configuration

The following table shows vcpkg triplets often relevant for IP cameras:

| Architecture | Description                                  | Triplet        | CMAKE_SYSTEM_PROCESSOR |
|--------------|----------------------------------------------|----------------|------------------------|
| `armhf`      | ARM 32-bit with hardware floating-point      | `arm-linux`    | `arm`                  |
| `arm64`      | ARM 64-bit                                   | `arm64-linux`  | `aarch64`              |
| `mips64`     | MIPS 64-bit                                  | `mips64-linux` | `mips64`               |

**Example: ARM 32-bit Linux**
```
export CC=arm-linux-gnueabihf-gcc
export CXX=arm-linux-gnueabihf-g++
cd examples/libdatachannel
mkdir -p build/linux_arm32_crosscompile
cd build/linux_arm32_crosscompile
cmake -DCMAKE_TOOLCHAIN_FILE=$(pwd)/../../../../3rdparty/vcpkg/scripts/buildsystems/vcpkg.cmake \
      -DVCPKG_TARGET_TRIPLET=arm-linux \
      -DCMAKE_SYSTEM_NAME=Linux \
      -DCMAKE_SYSTEM_PROCESSOR=arm \
      -DCMAKE_INSTALL_PREFIX=$(pwd)/install ../../
cmake --build . --target install
```

### 3. Creating a custom triplet for unsupported architectures

If you need to build for an architecture that is not officially supported or part of the community repository, it is quite straight forward to make your own. For instance, to support `mips32`, create a `triplets` subdirectory in the examples/libdatachannel directory (at the same level as `CMakePresets.json`).

```
mkdir triplets
```

In that directory, create a file named `mips32-linux.cmake` with the following contents:

```
set(VCPKG_TARGET_ARCHITECTURE mips32)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)
```

Then add `-DVCPKG_OVERLAY_TRIPLETS=<path>/triplets` to your build command and select the new target triplet with `-DVCPKG_TARGET_TRIPLET=mips32-linux`:

```
export CC="mips-gcc720-glibc226/bin/mips-linux-gnu-gcc -muclibc"
export CXX="mips-gcc720-glibc226/bin/mips-linux-gnu-g++ -muclibc"
cd examples/libdatachannel
mkdir -p build/linux_mips32_crosscompile
cd build/linux_mips32_crosscompile
cmake -DCMAKE_TOOLCHAIN_FILE=$(pwd)/../../../../3rdparty/vcpkg/scripts/buildsystems/vcpkg.cmake \
      -DVCPKG_OVERLAY_TRIPLETS=$(pwd)/../../triplets \
      -DVCPKG_TARGET_TRIPLET=mips32-linux \
      -DCMAKE_SYSTEM_NAME=Linux \
      -DCMAKE_SYSTEM_PROCESSOR=mips \
      -DCMAKE_INSTALL_PREFIX=$(pwd)/install ../../
cmake --build . --target install
```
