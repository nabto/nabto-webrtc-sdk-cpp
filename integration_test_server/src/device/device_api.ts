import { TestDevices, testDevicesPlugin } from "./DeviceTestInstance";
import { Elysia, t } from 'elysia'
import { RoutingUnionScheme } from "../WebsocketProtocolDataTypes";
import bearer from "@elysiajs/bearer";

const errorType = t.Object({
  message: t.Optional(t.String())
}, {description: "failure"});

export const deviceHttp = new Elysia()
  .use(testDevicesPlugin)
  .use(bearer())
  .post("/v1/device/connect", async ({ testDevices, error, body: { productId, deviceId }, bearer }) => {
    const test = testDevices.getDeviceByProductId(productId);
    if (!test) {
      return error(404, { message: "No Such Test" });
    }

    if (test.options.failHttp === true) {
      return error(400, { message: "Bad request" });
    }

    if (!bearer) {
      return error(401, { message: "Unauthorized" });
    }

    if (bearer != test.accessToken) {
      return error(403, {message: "Access denied"})
    }

    let extraData = {}
    if (test.options.extraDeviceConnectResponseData === true) {
      console.log("adding extra data")
      extraData = {"extra_field_in_the_response": "for testing"}
    }
    return { signalingUrl: `ws://127.0.0.1:13745/device-ws/${test.testId}`, ...extraData }
  }, {
    body: t.Object({
      deviceId: t.String(),
      productId: t.String(),
    }),
    headers: t.Object({
      authorization: t.String()
    }),
    response: {
      200: t.Object({
        signalingUrl: t.String(),
        extra_field_in_the_response: t.Optional(t.String())
      }, { description: "success"}),
      400: errorType,
      404: errorType,
      401: errorType,
      403: errorType
    }
  })

export const deviceWs = new Elysia()
  .use(testDevicesPlugin)
  .ws("/device-ws/:testId", {
    async beforeHandle({ testDevices, params, error }) {
      const test = testDevices.getByTestId(params.testId);
      if (!test) {
        console.log(`The test with id ${params.testId} was not found`)
        return error(404, "Test not Found")
      }
      if (test.options.failWs === true) {
        console.log(`Deliberately failing the websocket request for the test with id ${params.testId}`)
        return error(404, "Failing the ws request")
      }
    },
    async open(ws) {
      const testId = ws.data.params.testId;
      const testDevices = ws.data.testDevices;
      const test = testDevices.getByTestId(testId);
      if (!test) {
        ws.close(4040, `No such test with id ${testId}.`)
        return;
      }
      test.deviceConnected((msg: string) => { console.log("sending ws message"); ws.send(msg) }, (errorCode: number, message: string) => { ws.close(errorCode, message) });
    },
    message(ws, message) {
      const test = ws.data.testDevices.getByTestId(ws.data.params.testId)
      if (test) {
        test.handleWsMessage(message)
      }
    },
    body: RoutingUnionScheme
  });
