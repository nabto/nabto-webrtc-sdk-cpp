import { Elysia, t } from "elysia";
import { TestClientOptionsSchema, testClientsPlugin } from "./TestClientInstance";

export const clientTestApi = new Elysia({ prefix: "/test/client" })
  .use(testClientsPlugin)
  .post("/", async ({ body, testClients }) => {
    const testClient = testClients.createTestClient(body)
    return {
      productId: testClient.productId,
      deviceId: testClient.deviceId,
      testId: testClient.testId,
      endpointUrl: "http://127.0.0.1:13745"
    }
  }, {
    body: TestClientOptionsSchema,
    response: {
      200: t.Object(
        {
          productId: t.String(),
          deviceId: t.String(),
          endpointUrl: t.String(),
          testId: t.String()
        }, { description: "success"}
      ),
      400: t.String({ description: "failure"})
    }
  })
  .post("/:testId/connect-device", async ({ error, params, testClients }) => {
    const test = testClients.getByTestId(params.testId);
    if (!test) {
      return error(404, "No such test id")
    }
    await test.connectDevice();
    return {}
  }, {
    response: {
      200: t.Object({}, { description: "success"}
      ),
      404: t.String({ description: "failure"})
    }
  })
  .post("/:testId/disconnect-device", async ({ error, params, testClients }) => {
    const test = testClients.getByTestId(params.testId);
    if (!test) {
      return error(404, "No such test id")
    }
    await test.disconnectDevice();
    return {}
  }, {
    response: {
      200: t.Object({}, { description: "success"}
      ),
      404: t.String({ description: "failure"})
    }
  })
  .post("/:testId/drop-device-messages", async ({ error, params, testClients }) => {
    const test = testClients.getByTestId(params.testId);
    if (!test) {
      return error(404, "No such test id")
    }
    await test.dropDeviceMessages();
    return {}
  }, {
    response: {
      200: t.Object({}, { description: "success"}
      ),
      404: t.String({ description: "failure"})
    }
  })
  .post("/:testId/disconnect-client", async ({ error, params, testClients }) => {
    const test = testClients.getByTestId(params.testId);
    if (!test) {
      return error(404, "No such test id")
    }
    test.disconnectClient();
    return {}
  }, {
    response: {
      200: t.Object({}, { description: "success"}
      ),
      404: t.String({ description: "failure"})
    }
  })
  .post("/:testId/drop-client-messages", async ({ error, params, testClients }) => {
    const test = testClients.getByTestId(params.testId);
    if (!test) {
      return error(404, "No such test id")
    }
    test.dropClientMessages();
    return {}
  }, {
    response: {
      200: t.Object({}, { description: "success"}
      ),
      404: t.String({ description: "failure"})
    }
  })

  .post("/:testId/wait-for-device-messages", async ({ error, params, testClients, body }) => {
    const test = testClients.getByTestId(params.testId);
    if (!test) {
      return error(404, "No such test id")
    }
    const messages = await test.waitForMessages(body.messages, body.timeout);
    return {
      messages: messages
    }
  },
    {
      body: t.Object({
        messages: t.Array(t.String()),
        timeout: t.Number()
      }),
      response: {
        200: t.Object({
          messages: t.Optional(t.Array(t.String()))
        }, { description: "success"}
      ),
      404: t.String({ description: "failure"})
      }
    }
  )
  .post("/:testId/send-device-messages", async ({ params, body, testClients, error }) => {
    const test = testClients.getByTestId(params.testId);
    if (!test) {
      return error(404, "No such test id")
    }
    await test.deviceSendMessages(body.messages);
    return {}
  },
    {
      body: t.Object({
        messages: t.Array(t.String())
      }),
      response: {
        200: t.Object({}, { description: "success"}),
        404: t.String({ description: "failure"})
      }
    }
  )
  .post("/:testId/send-device-error", async ({ params, body, testClients, error }) => {
    const test = testClients.getByTestId(params.testId);
    if (!test) {
      return error(404, "No such test id")
    }
    await test.deviceSendError(body.errorCode, body.errorMessage);
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
  .post("/:testId/send-new-message-type", async ({ params, testClients, error }) => {
    const test = testClients.getByTestId(params.testId);
    if (!test) {
      return error(404, "No such test id")
    }
    await test.sendNewMessageType();
    return {}
  },
  {
    body: t.Object({ }),
    response: {
      200: t.Object({}, { description: "success"}),
      404: t.String({ description: "failure"})
    }
  }
  )
  .delete("/:testId", async ({ params, testClients }) => {
    testClients.deleteTest(params.testId);
    return {
    }
  }, {
    response: {
      200: t.Object({}, { description: "success"})
    }
  })
