import { TestClients, testClientsPlugin } from "./TestClientInstance";
import { Elysia, t } from 'elysia'
import { RoutingUnionScheme } from "../WebsocketProtocolDataTypes";

const errorType = t.Object({
  message: t.Optional(t.String())
}, {description: "failure"});

export const clientHttp = new Elysia()
  .use(testClientsPlugin)
  .post("/v1/client/connect", async ({ testClients, error, body: { productId, deviceId } }) => {
    const test = testClients.getClientByProductId(productId);
    if (!test) {
      return error(404, { message: "No Such Test" });
    }

    if (test.options.failHttp === true) {
      return error(400, { message: "Bad request" });
    }
    let extraData = {}
    if (test.options.extraClientConnectResponseData === true) {
      console.log("adding extra data")
      extraData = {"extra_field_in_the_response": "for testing"}
    }
    return { signalingUrl: `ws://127.0.0.1:13745/client-ws/${test.testId}`, deviceOnline: test.isDeviceConnected(), channelId: test.testId, ...extraData }
  }, {
    body: t.Object({
      deviceId: t.String(),
      productId: t.String(),
    }),
    response: {
      200: t.Object({
        signalingUrl: t.String(),
        deviceOnline: t.Boolean(),
        channelId: t.String(),
        extra_field_in_the_response: t.Optional(t.String())
      }, { description: "success"}),
      400: errorType,
      404: errorType,
    }
  })

export const clientWs = new Elysia()
  .use(testClientsPlugin)
  .ws("/client-ws/:testId", {
    async beforeHandle({ testClients, params, error }) {
      const test = testClients.getByTestId(params.testId);
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
      const testClients = ws.data.testClients;
      const test = testClients.getByTestId(testId);
      if (!test) {
        ws.close(4040, `No such test with id ${testId}.`)
        return;
      }
      test.clientConnected((msg: string) => { console.log(`sending ws message ${msg}`); ws.send(msg) }, (errorCode: number, message: string) => { ws.close(errorCode, message) });
    },
    message(ws, message) {
      const test = ws.data.testClients.getByTestId(ws.data.params.testId)
      if (test) {
        try {
          console.log(`Got message ${JSON.stringify(message)}`)
          test.handleWsMessage(message);
        } catch (e) {
          console.error(`${e} message: ${JSON.stringify(message)}`);
        }
      }
    },
    body: RoutingUnionScheme
  });
