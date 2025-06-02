import { ReliabilityAck, ReliabilityMessage, ReliabilityTypes, ReliabilityUnion } from './WebsocketProtocolDataTypes'
/**
 * The basic protocol:
 *  * Each time a message is received an ack is sent
 *  * All data is received ordered, but with possible duplicates.
 *  * The first message has the sequence number 0
 */
interface ReliabilityEventMap {
  message: string,
}

export class Reliability {
  // Recv Sequence number is the next sequence number we expect to receive.
  recvSeq: number = 0

  // Send state
  sendSeq: number = 0 // the next sequence number for a message to be sent.
  // List of messages which has not been Acked by the remote peer yet.
  unackedMessages: Array<ReliabilityMessage> = new Array<ReliabilityMessage>()

  /*
   * @param onRoutingMessage  A function which is called when the Reliability layer wants to send a message to the routing layer.
   */
  constructor(private sendRoutingMessage: (message: ReliabilityUnion) => void) {
  }

  /**
   * Send a reliable message.
   *
   * This function wraps the string we want to send into an object containing
   * the message, a sequence number and a type to distinguish it from an ACK.
   *
   * The message is then send on the websocket.
   *
   * @param message The message to send.
   */
  sendReliableMessage(message: unknown): void {
    const encoded: ReliabilityMessage = {
      type: ReliabilityTypes.DATA,
      seq: this.sendSeq,
      data: message
    }
    this.sendSeq++;
    this.unackedMessages.push(encoded);
    this.sendRoutingMessage(encoded);
  }

  /**
   * This function is called when a routing message is received from the
   * websocket.
   *
   * @param message the reliability message to use
   * @return if the received message is an expected ReliabilityMessage the
   * function returns the payload from that packet.
   */
  handleRoutingMessage(message: ReliabilityUnion) : unknown | undefined {
    if (message.type === ReliabilityTypes.ACK) {
      this.handleAck(message)
      return undefined;
    } else if (message.type === ReliabilityTypes.DATA) {
      return this.handleReliabilityMessage(message);
    }
    return undefined;
  }

  /**
   * Handle a routing message. if the payload was expected the function returns
   * the payload of the message.
   * @param message
   * @returns
   */
  private handleReliabilityMessage(message: ReliabilityMessage) : unknown | undefined {

    if (message.seq <= this.recvSeq) {
      // This is either an expected message or a message from the past
      // which has been retransmitted, send an ack. We should not send
      // acks for messages from the future as we need to have the other
      // end to retransmit them in the correct order.
      this.sendAck(message.seq);
    }

    // Check if the message is a duplicate from the past or from the future.
    if (message.seq != this.recvSeq) {
      // This is an out of order message, this can happen in some cases,
      // when peers reconnects to the signaling service. The other peer
      // will resend the messages in the correct order when it detects
      // that this client has been reconnected.
      console.debug(`Received a message with seq: ${message.seq}, the expected recvSeq is ${this.recvSeq}. This is expected when peers reconnects or initially when they connect.`)
      return undefined;
    }

    this.recvSeq++;
    return message.data;
  }

  /**
   * Handle an ACK
   *
   * If the ACK acknowledges the first item in our list of unacknowledged
   * messages, the item is removed from the list.
   *
   * @param seq The sequence number of the ack.
   * @returns
   */
  private handleAck(ack: ReliabilityAck) {
    // The ack will always be for the first item in the unackedMessages array
    const firstItem = this.unackedMessages[0];
    if (firstItem === undefined) {
      // This could happen if one peer reconnects, but an old websocket
      // connection has a stale ACK packet which is sent after the new
      // connection has been made and new messages has been retransmitted.
      console.info("Got an ack but we have no unacked messages.");
      return;
    }
    if (firstItem.seq != ack.seq) {
      console.info(`Got an ack for the sequence number ${ack.seq} but the first item in the unacked messages array has the sequence number ${firstItem.seq}`)
      return;
    }

    // Ack the first element in the list, by removing it from the list.
    this.unackedMessages.shift();
  }

  /**
   * Send unacked messages. This function is called when an event is detected
   * which makes us believe that the remote peer has not received our
   * outstanding unacknowledged packets.
   */
  private sendUnackedMessages() {
    // resend all unacknowledged packets again
    this.unackedMessages.forEach((msg) => {
      this.sendRoutingMessage(msg);
    })
  }

  /**
   * Send an ack to the remote peer.
   * @param seq
   */
  private sendAck(seq: number) {
    console.info(`Sending ACK with seq ${seq}`)
    const ack : ReliabilityAck = { type: ReliabilityTypes.ACK, seq: seq }
    this.sendRoutingMessage(ack);
  }

  /**
   * If it has been detected that the remote peer has been connected or
   * reconnected.
   */
  handlePeerConnected() {
    this.sendUnackedMessages()
  }

  /**
   * If the underlying websocket connection has connected/reconnected
   */
  handleConnect() {
    this.sendUnackedMessages();
  }

  /**
   * return true if the message is the first reliable message for a flow of
   * messages. This is used to detect legacy packets for older connection ids
   * for older device instances. Eg. to detect packets from old connections
   * after a device restart.
   */
  isInitialMessage(message: ReliabilityUnion): boolean {
    return message.type === ReliabilityTypes.DATA && message.seq === 0;
  }
}
