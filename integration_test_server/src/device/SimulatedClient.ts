import { Type as t, Static } from "@sinclair/typebox"
import { Value } from '@sinclair/typebox/value';

import { ReliabilityAck, ReliabilityMessage, ReliabilityTypes, ReliabilityUnion, ReliabilityUnionScheme, Routing, RoutingError, RoutingMessage, RoutingMessageScheme, RoutingProtocolError, RoutingTypes, RoutingUnionScheme } from '../WebsocketProtocolDataTypes'
import { Reliability } from "../Reliability";
import { EventEmitter } from 'node:events'
import { randomUUID } from "node:crypto";

/**
 * This is an implementation of a device which can handle a single client
 */
export class SimulatedClient {
  reliabilityRecvSeq: number = 0;
  reliabilitySendSeq: number = 0;
  reliabilityUnackedMessages: Array<ReliabilityMessage> = new Array<ReliabilityMessage>()

  reliability: Reliability;

  receivedMessages: Array<string> = new Array();
  receivedError?: RoutingProtocolError;
  eventEmitter: EventEmitter = new EventEmitter();

  connected: boolean = true;
  dropIncomingMessages: boolean = false;
  clientId: string = randomUUID()

  constructor(public channelId: string, private wsSender: (msg: Routing) => void) {
    this.reliability = new Reliability((message: ReliabilityUnion) => {
      if (this.connected) {
        this.wsSender({
          type: RoutingTypes.MESSAGE,
          channelId: this.channelId,
          message: JSON.stringify(message)
        })
      }
    })
  }

  connect() {
    if (this.connected === false) {
      this.connected = true;
      this.dropIncomingMessages = false;
      this.wsSender({
        type: RoutingTypes.PEER_CONNECTED,
        channelId: this.channelId
      })
      this.reliability.handleConnect();
    }
  }

  disconnect() {
    this.connected = false;
  }

  async dropMessages() {
    this.dropIncomingMessages = true;
  }

  handleWsMessage(message: Routing) {
    if (this.dropIncomingMessages) {
      console.log("Dropping incoming message")
      return;
    }
    if (!this.connected) {
      this.wsSender({
        type: RoutingTypes.PEER_OFFLINE,
        channelId: this.channelId
      })
    } else {
      this.handleRouting(message);
    }
  }
  handleRouting(routing: Routing) {
    if (routing.type === RoutingTypes.MESSAGE) {
      const json = JSON.parse(routing.message);
      const reliability = Value.Decode(ReliabilityUnionScheme, json);
      const reliableMessage = this.reliability.handleRoutingMessage(reliability);
      if (reliableMessage) {
        this.handleReliableMessage(reliableMessage);
      }
    } else if (routing.type === RoutingTypes.ERROR) {
      this.handleError(routing);
    }
  }

  handlePeerConnected() {
    this.reliability.handlePeerConnected();
  }

  handleReliableMessage(message: string) {
    this.receivedMessages.push(message);
    this.eventEmitter.emit("message", message)
  }

  handleError(err: RoutingError) {
    this.receivedError = {errorCode: err.errorCode, errorMessage: err.errorMessage};
    this.eventEmitter.emit("test_error", this.receivedError);

  }

  hasReceivedMessages(messages: string[]): boolean {
    if (JSON.stringify(this.receivedMessages) === JSON.stringify(messages)) {
      return true;
    }
    return false;
  }

  async waitForMessages(messages: string[], timeout: number): Promise<string[]> {
    if (this.hasReceivedMessages(messages)) {
      return this.receivedMessages;
    }
    const promise = new Promise((resolve, reject) => {
      this.eventEmitter.on("message", (message: string) => {
        if (this.hasReceivedMessages(messages)) {
          resolve(true);
        }
      })
      setTimeout(() => { resolve(true) }, timeout);
    })
    await promise
    return this.receivedMessages;
  }

  async sendMessages(messages: string[]) {
    for (const msg of messages) {
      await this.reliability.sendReliableMessage(msg);
    }
  }

  async sendError(errorCode: string, errorMessage?: string) {
    await this.wsSender({
      type: RoutingTypes.ERROR,
      channelId: this.channelId,
      errorCode: errorCode,
      errorMessage: errorMessage
    })
  }

  async waitForError(timeout: number): Promise<RoutingProtocolError> {
    if (this.receivedError) {
      return this.receivedError;
    }
    const promise = new Promise<void>((resolve, reject) => {
      this.eventEmitter.on("test_error", (err: RoutingProtocolError) => {
        resolve();
      })
      setTimeout(() => {
        reject("Timeout");
      }, timeout);
    })
    await promise;
    if (this.receivedError) {
      return this.receivedError;
    } else {
      throw new Error("Not Possible");
    }

  }
}
