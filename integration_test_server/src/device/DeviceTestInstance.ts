import { ServerWebSocket } from "bun";

import { Elysia, t, Static } from 'elysia'
import { SimulatedClient } from "./SimulatedClient";
import { Routing, RoutingProtocolError, RoutingTypes } from "../WebsocketProtocolDataTypes";
import { randomUUID } from "node:crypto";

const messageType = t.Object({
  type: t.String(),
  authorized: t.Optional(t.Boolean()),
  channelId: t.Optional(t.String())
})

type Message = Static<typeof messageType>

export const TestDeviceOptionsSchema = t.Object({
  failHttp: t.Optional(t.Boolean()),
  failWs: t.Optional(t.Boolean()),
  extraDeviceConnectResponseData: t.Optional(t.Boolean())
});

export type TestDeviceOptions = Static<typeof TestDeviceOptionsSchema>

type wsCloseCallback = (errorCode: number, message: string) => void
type wsSendMessageCallback = (message: string) => void

class DeviceTestInstance {
  channelId: string = crypto.randomUUID();
  productId: string = crypto.randomUUID();
  deviceId: string = crypto.randomUUID();
  testId: string = this.productId;
  accessToken: string = crypto.randomUUID();

  clients = new Map<string, SimulatedClient>();

  droppingDeviceMessages : boolean = false

  wsSender?: wsSendMessageCallback
  wsClose?: wsCloseCallback
  constructor(public options: TestDeviceOptions) {
  }
  deviceConnected(wsSender: wsSendMessageCallback, wsClose: wsCloseCallback) {
    this.wsSender = wsSender;
    this.wsClose = wsClose;
    this.droppingDeviceMessages = false;
    this.clients.forEach((client) => {
      client.handlePeerConnected();
    })
  }

  async createClient() : Promise<string> {
    const channelId = randomUUID()
    const client = new SimulatedClient(channelId, (msg: Routing) => {
      this.sendMessage(msg);
    });
    this.clients.set(client.clientId, client);
    return client.clientId
  }

  async connectClient(clientId: string) {
    const client = this.clients.get(clientId);
    client?.connect();
  }

  disconnectClient(clientId: string) {
    const client = this.clients.get(clientId);
    client?.disconnect();
  }

  async dropClientMessages(clientId: string) {
    const client = this.clients.get(clientId);
    await client?.dropMessages();
  }

  async disconnectDevice() {
    this.wsClose?.(4042, "Disconnected by the test")
  }
  async dropDeviceMessages() {
    this.droppingDeviceMessages = true;
  }
  async sendNewMessageType() {
    this.wsSender?.(JSON.stringify({"type": "NEW_MESSAGE_TYPE", "new_message_type_field": "data"}))
  }

  sendMessage(msg: Routing) {
    this.wsSender?.(JSON.stringify(msg));
  }

  async waitForMessages(clientId: string, messages: unknown[], timeout: number): Promise<unknown[]>
  {
    const client = this.clients.get(clientId);
    if (!client) {
      throw new Error("No such simulated client");
    }
    return await client.waitForMessages(messages, timeout)
  }
  async waitForError(clientId: string, timeout: number): Promise<RoutingProtocolError>
  {
    const client = this.clients.get(clientId);
    if (!client) {
      throw new Error("No such simulated client");
    }
    return await client.waitForError(timeout)
  }
  getReceivedMssages(clientId: string): unknown[] {
    const client = this.clients.get(clientId);
    if (!client) {
      throw new Error("No such simulated client");
    }
    return client.receivedMessages;
  }

  handleWsMessage(msg: Routing) {
    if (this.droppingDeviceMessages) {
      return;
    }
    if (msg.type === RoutingTypes.PING) {
      this.sendMessage({ type: RoutingTypes.PONG })
    } else if (msg.type === RoutingTypes.PONG) {
      // discard pongs
    } else {
      this.clients.forEach((client) => {
        if (client.channelId === msg.channelId) {
          client.handleWsMessage(msg);
        }
      })
    }
  }

  async clientSendMessages(clientId: string, messages: unknown[])
  {
    const client = this.clients.get(clientId);
    if (!client) {
      throw new Error("No such simulated client");
    }
    await client.sendMessages(messages);
  }

  async clientSendError(clientId: string, errorCode: string, errorMessage?: string)
  {
    const client = this.clients.get(clientId);
    if (!client) {
      throw new Error("No such simulated client");
    }
    await client.sendError(errorCode, errorMessage);
  }

  close() {

  }
}


export class TestDevices {
  idCounter: number = 0;
  devices: Array<DeviceTestInstance> = new Array<DeviceTestInstance>()

  createTest(options: TestDeviceOptions) : DeviceTestInstance {
    const testDevice = new DeviceTestInstance(options)
    this.devices.push(testDevice);
    return testDevice;
  }

  getDeviceByProductId(productId: string): DeviceTestInstance | undefined {
    for (let n = 0; n < this.devices.length; n++) {
      const c = this.devices[n];
      if (c.productId === productId) {
        return c;
      }
    }
    return undefined;
  }

  getByTestId(testId: string): DeviceTestInstance | undefined {

    for (let n = 0; n < this.devices.length; n++) {
      const c = this.devices[n];
      if (c.testId === testId) {
        return c;
      }
    }
    return undefined;
  }
  deleteTest(testId: string) {
    const test = this.getByTestId(testId);
    test?.close();
    this.devices = this.devices.filter((a) => a.testId !== testId)
  }
}


export const testDevicesPlugin = new Elysia({name: "testDevices"})
  .decorate("testDevices", new TestDevices())
