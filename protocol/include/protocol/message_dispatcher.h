#ifndef MESSAGE_DISPATCHER_H
#define MESSAGE_DISPATCHER_H

#include <variant>

#include "binary_codec.h"
#include "messages.h"

namespace protocol
{
    class MessageDispatcher
    {
    public:
        using MessageVariant = std::variant<NewOrderMessage, CancelOrderMessage, ModifyOrderMessage>;

        static MessageVariant Deserialize(const void *data, size_t length)
        {
            auto header = BinaryCodec::ParseHeader(data, length);

            switch (header.msg_type)
            {
                case MessageType::NEW_ORDER:
                {
                    return BinaryCodec::Decode<NewOrderMessage>(data, length);
                }

                case MessageType::CANCEL_ORDER:
                {
                    return BinaryCodec::Decode<CancelOrderMessage>(data, length);
                }

                case MessageType::MODIFY_ORDER:
                {
                    return BinaryCodec::Decode<ModifyOrderMessage>(data, length);
                }

                default: 
                {
                    throw std::runtime_error("Unknown message type");
                }
            }
        }

    };

} // namespace protocol

#endif // MESSAGE_DISPATCHER_H