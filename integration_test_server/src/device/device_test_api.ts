import { Elysia, t } from "elysia";
import { TestDeviceOptionsSchema, testDevicesPlugin } from "./DeviceTestInstance";

export const deviceTestApi = new Elysia({ prefix: "/test/device" })
  .use(testDevicesPlugin)
  .post("/", async ({ body, testDevices }) => {
    const testClient = testDevices.createTest(body)
    return {
      productId: testClient.productId,
      deviceId: testClient.deviceId,
      testId: testClient.testId,
      endpointUrl: "http://127.0.0.1:13745",
      accessToken: testClient.accessToken
    }
  }, {
    parse: 'application/json', // This is supposed to work but does not: https://github.com/elysiajs/elysia-swagger/issues/192
    type: 'application/json' as never, // So we have this instead
    body: TestDeviceOptionsSchema,
    response: {
      200: t.Object(
        {
          productId: t.String(),
          deviceId: t.String(),
          endpointUrl: t.String(),
          testId: t.String(),
          accessToken: t.String()
        }, { description: "success"}
      ),
      400: t.String({ description: "failure"})
    }
  })

  .post("/:testId/clients", async ({ error, params, testDevices }) => {
    const test = testDevices.getByTestId(params.testId);
    if (!test) {
      return error(404, "No such test id")
    }
    const clientId = await test.createClient();
    return {
      clientId: clientId
    }
  }, {
    response: {
      200: t.Object({
        clientId: t.String()
      }, { description: "success"}),
      404: t.String({ description: "failure"})
    }
  })
  .post("/:testId/clients/:clientId/connect", async ({ error, params, testDevices }) => {
    const test = testDevices.getByTestId(params.testId);
    if (!test) {
      return error(404, "No such test id")
    }
    await test.connectClient(params.clientId);
    return {}
  }, {
    response: {
      200: t.Object({}, { description: "success"}),
      404: t.String({ description: "failure"})
    }
  })
  .post("/:testId/clients/:clientId/disconnect", async ({ error, params, testDevices }) => {
    const test = testDevices.getByTestId(params.testId);
    if (!test) {
      return error(404, "No such test id")
    }
    await test.disconnectClient(params.clientId);
    return {}
  }, {
    response: {
      200: t.Object({}, { description: "success"}),
      404: t.String({ description: "failure"})
    }
  })
  .post("/:testId/clients/:clientId/drop-client-messages", async ({ error, params, testDevices, body }) => {
    const test = testDevices.getByTestId(params.testId);
    if (!test) {
      return error(404, "No such test id")
    }
    await test.dropClientMessages(params.clientId);
    return {}
  }, {
    response: {
      200: t.Object({}, { description: "success"}),
      404: t.String({ description: "failure"})
    }
  })
  .post("/:testId/disconnect-device", async ({ error, params, testDevices }) => {
    const test = testDevices.getByTestId(params.testId);
    if (!test) {
      return error(404, "No such test id")
    }
    test.disconnectDevice();
    return {}
  }, {
    response: {
      200: t.Object({}, { description: "success"}),
      404: t.String({ description: "failure"})
    }
  })
  .post("/:testId/drop-device-messages", async ({ error, params, testDevices }) => {
    const test = testDevices.getByTestId(params.testId);
    if (!test) {
      return error(404, "No such test id")
    }
    test.dropDeviceMessages();
    return {}
  }, {
    response: {
      200: t.Object({}, { description: "success"}),
      404: t.String({ description: "failure"})
    }
  })

  .post("/:testId/clients/:clientId/wait-for-messages", async ({ error, params, testDevices, body }) => {
    const test = testDevices.getByTestId(params.testId);
    if (!test) {
      return error(404, "No such test id")
    }
    const messages = await test.waitForMessages(params.clientId, body.messages, body.timeout);
    return {
      messages: messages
    }
  },
    {
      parse: 'application/json', // This is supposed to work but does not: https://github.com/elysiajs/elysia-swagger/issues/192
      type: 'application/json' as never, // So we have this instead
      body: t.Object({
        messages: t.Array(t.Unknown()),
        timeout: t.Number()
      }),
      response: {
        200: t.Object({
          messages: t.Optional(t.Array(t.Unknown()))
        }, { description: "success"}),
        404: t.String({ description: "failure"})
      }
    }
  )
  .post("/:testId/clients/:clientId/send-messages", async ({ params, body, testDevices, error }) => {
    const test = testDevices.getByTestId(params.testId);
    if (!test) {
      return error(404, "No such test id")
    }
    await test.clientSendMessages(params.clientId, body.messages);
    return {}
  },
    {
      parse: 'application/json', // This is supposed to work but does not: https://github.com/elysiajs/elysia-swagger/issues/192
      type: 'application/json' as never, // So we have this instead
      body: t.Object({
        messages: t.Array(t.Unknown())
      }),
      response: {
        200: t.Object({}, { description: "success"}),
        404: t.String({ description: "failure"})
      }
    }
  )
  .post("/:testId/clients/:clientId/send-error", async ({ params, body, testDevices, error }) => {
    const test = testDevices.getByTestId(params.testId);
    if (!test) {
      return error(404, "No such test id")
    }
    await test.clientSendError(params.clientId, body.errorCode, body.errorMessage);
    return {}
  },
    {
      body: t.Object({
        errorCode: t.String(),
        errorMessage: t.Optional(t.String()),
      }),
      response: {
        200: t.Object({}, { description: "success"}),
        404: t.String({ description: "failure"})
      }
    }
  )
  .post("/:testId/send-new-message-type", async ({ params, testDevices, error }) => {
    const test = testDevices.getByTestId(params.testId);
    if (!test) {
      return error(404, "No such test id")
    }
    await test.sendNewMessageType();
    return {}
  },
  {
    response: {
      200: t.Object({}, { description: "success"}),
      404: t.String({ description: "failure"})
    }
  }
  )
  .delete("/:testId", async ({ params, testDevices }) => {
    testDevices.deleteTest(params.testId);
    return {
    }
  }, {
    response: {
      200: t.Object({}, { description: "success"})
    }
  })
  .post("/:testId/clients/:clientId/wait-for-error", async ({ error, params, testDevices, body }) => {
    const test = testDevices.getByTestId(params.testId);
    if (!test) {
      return error(404, "No such test id")
    }
    console.log("body timeout: ", body.timeout);
    const err = await test.waitForError(params.clientId, body.timeout);
    return err;
  },
    {
      parse: 'application/json', // This is supposed to work but does not: https://github.com/elysiajs/elysia-swagger/issues/192
      type: 'application/json' as never, // So we have this instead
      body: t.Object({
        timeout: t.Number()
      }),
      response: {
        200: t.Object({
          errorCode: t.String(),
          errorMessage: t.Optional(t.String()),
        }, { description: "success"}),
        404: t.String({ description: "failure"})
      }
    }
  )
