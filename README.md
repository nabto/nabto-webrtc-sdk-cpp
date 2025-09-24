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
   -k key.pem --secret <secret>`
```

Run the RTSP device:

```
./build/debug/install/bin/webrtc_device_rtsp -d <deviceId> -p <productId> \
   -k key.pem --secret <secret> \
   -r rtsp://127.0.0.1:8554/video`
```

## Limitations

The libdatachannel WebRTC library used by the examples does not currently support offers generated with `restartIce()` due to this [issue](https://github.com/paullouisageneau/libdatachannel/issues/545). This means if one of the peers experiences a network failure or switches to a different network, any open WebRTC connections cannot be renegotiated. Instead the connection has to be recreated from scratch.
