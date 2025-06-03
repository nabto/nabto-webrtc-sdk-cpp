# Nabto WebRTC SDK C++

## Building and testing

Build the SDK using one of the cmake presets, eg.:

```
cmake --workflow --preset test
```

Then run unittests with:

```
./build/test/nabto_signaling_test
```

Then start the integration test server:

```
cd ../integration_test_server/
bun dev
```

Then run integration tests:

```
./build/test/nabto_integration_test
```

Then build the SDK with clang tidy to lint the code:

```
cmake --workflow --preset clang_tidy
```
