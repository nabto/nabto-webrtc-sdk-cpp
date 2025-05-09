import { ServerWebSocket } from "bun";

import { Elysia, t, Static } from 'elysia'
import { SimulatedDevice } from "./SimulatedDevice";
import { Routing, RoutingTypes } from "../WebsocketProtocolDataTypes";

const messageType = t.Object({
  type: t.String(),
  authorized: t.Optional(t.Boolean()),
  channelId: t.Optional(t.String())
})

type Message = Static<typeof messageType>

export const TestClientOptionsSchema = t.Object({
  failHttp: t.Optional(t.Boolean()),
  failWs: t.Optional(t.Boolean()),
  extraClientConnectResponseData: t.Optional(t.Boolean())
});

export type TestClientOptions = Static<typeof TestClientOptionsSchema>

type wsCloseCallback = (errorCode: number, message: string) => void
type wsSendMessageCallback = (message: string) => void

class TestClientInstance {
  channelId: string = crypto.randomUUID();
  productId: string = crypto.randomUUID();
  deviceId: string = crypto.randomUUID();
  testId: string = this.productId;

  device: SimulatedDevice;

  droppingClientMessages : boolean = false

  wsSender?: wsSendMessageCallback
  wsClose?: wsCloseCallback
  constructor(public options: TestClientOptions) {
    this.device = new SimulatedDevice(this.channelId, (msg: Routing) => { this.sendMessage(msg) });
  }
  clientConnected(wsSender: wsSendMessageCallback, wsClose: wsCloseCallback) {
    this.wsSender = wsSender;
    this.wsClose = wsClose;
    this.droppingClientMessages = false;
    this.device.handlePeerConnected();
  }

  async connectDevice() {
    this.device.connect();
  }

  isDeviceConnected() {
    return this.device.connected;
  }

  disconnectDevice() {
    this.device.disconnect();
  }

  async dropDeviceMessages() {
    await this.device.dropMessages();
  }

  async disconnectClient() {
    this.wsClose?.(4042, "Disconnected by the test")
  }
  async dropClientMessages() {
    this.droppingClientMessages = true;
  }
  async sendNewMessageType() {
    this.wsSender?.(JSON.stringify({"type": "NEW_MESSAGE_TYPE", "new_message_type_field": "data"}))
  }

  sendMessage(msg: Routing) {
    this.wsSender?.(JSON.stringify(msg));
  }

  async waitForMessages(messages: string[], timeout: number): Promise<string[]>
  {
    return await this.device.waitForMessages(messages, timeout)
  }
  getReceivedMessages(): string[] {
    return this.device.receivedMessages;
  }

  handleWsMessage(msg: Routing) {
    if (this.droppingClientMessages) {
      return;
    }
    if (msg.type === RoutingTypes.PING) {
      this.sendMessage({ type: RoutingTypes.PONG })
    } else if (msg.type === RoutingTypes.PONG) {
      // discard pongs
    } else {
      this.device.handleWsMessage(msg);
    }
  }

  async deviceSendMessages(messages: string[]) {
    await this.device.sendMessages(messages);
  }

  async deviceSendError(errorCode: string, errorMessage?: string) {
    await this.device.sendError(errorCode, errorMessage);
  }

  close() {

  }
}


export class TestClients {
  idCounter: number = 0;
  clients: Array<TestClientInstance> = new Array<TestClientInstance>()

  createTestClient(options: TestClientOptions) {
    const testClient = new TestClientInstance(options)
    this.clients.push(testClient);
    return testClient;
  }

  getClientByProductId(productId: string): TestClientInstance | undefined {
    for (let n = 0; n < this.clients.length; n++) {
      const c = this.clients[n];
      if (c.productId === productId) {
        return c;
      }
    }
    return undefined;
  }

  getByTestId(testId: string): TestClientInstance | undefined {

    for (let n = 0; n < this.clients.length; n++) {
      const c = this.clients[n];
      if (c.testId === testId) {
        return c;
      }
    }
    return undefined;
  }
  deleteTest(testId: string) {
    const test = this.getByTestId(testId);
    test?.close();
    this.clients = this.clients.filter((a) => a.testId !== testId)
  }
}


export const testClientsPlugin = new Elysia({name: "testClients"})
  .decorate("testClients", new TestClients())
