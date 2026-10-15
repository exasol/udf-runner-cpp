#pragma once

namespace exasol::udf::v2
{

/// Result of a non-consuming readiness operation.
enum class MessageStatus
{
    MessageAvailable,
    NoMessage,
    TimedOut,
    Cancelled,
    PeerClosed,
    ConnectionError
};

/// Result status of a consuming or sending operation.
enum class OperationStatus
{
    Ok,
    TimedOut,
    Cancelled,
    PeerClosed,
    ProtocolError,
    TransportError
};

} // namespace exasol::udf::v2
