# Updating C++ API

1) Obtain the open API spec from swagger
2) run `docker run --rm -v "${PWD}:/local" openapitools/openapi-generator-cli generate -i /local/json.json -g cpp-restsdk -o /local/out/cpp`
3) copy the resulting api. eg.: `cp -r out/cpp ~/sandbox/nabto-webrtc-signaling-preview/sdk/cpp/integration-test/generated/`
