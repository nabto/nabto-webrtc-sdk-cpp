# Integration test server

The integration test server is a server which mocks the http and ws protocol the
client and device SDK uses when they are communicating with the Nabto WebRTC
Signaling Service. The provide a mock api for the HTTP and WS interaction
together with endpoints which is used to control the tests which is executed
against the system.

## How to

start it with `bun dev`

Run integration tests against it.

default host: http://localhost:13745
