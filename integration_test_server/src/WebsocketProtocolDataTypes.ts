import { t, Static } from 'elysia'

export enum RoutingTypes {
  MESSAGE = "MESSAGE",
  ERROR = "ERROR",
  PEER_CONNECTED = "PEER_CONNECTED",
  PEER_OFFLINE = "PEER_OFFLINE",
  PING = "PING",
  PONG = "PONG"

}

export const RoutingMessageScheme = t.Object({
  type: t.Literal(RoutingTypes.MESSAGE),
  message: t.Unknown(),
  channelId: t.String(),
  authorized: t.Optional(t.Boolean()),
})

export const RoutingErrorScheme = t.Object({
  type: t.Literal(RoutingTypes.ERROR),
  error: t.Object({
    code: t.String(),
    message: t.Optional(t.String()),
  }),
  channelId: t.String()
})

export const RoutingPeerConnectedScheme = t.Object({
  type: t.Literal(RoutingTypes.PEER_CONNECTED),
  channelId: t.String(),
})

export const RoutingPeerOfflineScheme = t.Object({
  type: t.Literal(RoutingTypes.PEER_OFFLINE),
  channelId: t.String(),
})

export const RoutingPingScheme = t.Object({
  type: t.Literal(RoutingTypes.PING)
})

export const RoutingPongScheme = t.Object({
  type: t.Literal(RoutingTypes.PONG)
})



export const RoutingUnionScheme = t.Union([RoutingMessageScheme, RoutingErrorScheme, RoutingPeerConnectedScheme, RoutingPeerOfflineScheme, RoutingPingScheme, RoutingPongScheme])

export type RoutingMessage = Static<typeof RoutingMessageScheme>
export type RoutingError = Static<typeof RoutingErrorScheme>
export type Routing = Static<typeof RoutingUnionScheme>

export enum ReliabilityTypes {
  ACK = "ACK",
  DATA = "DATA"
}

export const ReliabilityAckScheme = t.Object({
  type: t.Literal(ReliabilityTypes.ACK),
  seq: t.Number(),
})

export const ReliabilityMessageScheme = t.Object({
  type: t.Literal(ReliabilityTypes.DATA),
  seq: t.Number(),
  data: t.Unknown()
})

export const ReliabilityUnionScheme = t.Union([ReliabilityAckScheme, ReliabilityMessageScheme])

export type ReliabilityAck = Static<typeof ReliabilityAckScheme>
export type ReliabilityMessage = Static<typeof ReliabilityMessageScheme>
export type ReliabilityUnion = Static<typeof ReliabilityUnionScheme>


export interface RoutingProtocolError {errorCode: string, errorMessage?: string};
