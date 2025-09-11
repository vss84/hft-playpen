#ifndef BINARY_CODEC_H
#define BINARY_CODEC_H

#include <vector>

#include "messages.h"

namespace protocol
{
    class BinaryCodec
    {
    public:
        template<typename MessageType>
        static std::vector<uint8_t> Encode(const MessageType &msg)
        {
            std::vector<uint8_t> buffer(sizeof(MessageType));
            std::memcpy(buffer.data(), &msg, sizeof(MessageType));
            return buffer;
        }
        
        template<typename MessageType>
        static MessageType Decode(const void *data, size_t length)
        {
            if (length < sizeof(MessageType))
            {
                throw std::runtime_error("Insufficient data for message");
            }

            MessageType msg;
            std::memcpy(&msg, data, sizeof(MessageType));
            return msg;
        }
        
        static MessageHeader ParseHeader(const void *data, size_t length)
        {
            if (length < sizeof(MessageHeader))
            {
                throw std::runtime_error("Insufficient data for header");
            }

            MessageHeader header;
            std::memcpy(&header, data, sizeof(header));

            if (header.msg_length > length)
            {
                throw std::runtime_error("Incomplete message");
            }

            return header;
        }

        static double TicksToPrice(uint32_t ticks, double tick_size = 0.01)
        {
            return ticks * tick_size;
        }

        static uint32_t PriceToTicks(double price, double tick_size = 0.01)
        {
            return static_cast<uint32_t>(price / tick_size);
        }
    };

} // namespace protocol

#endif // BINARY_CODEC_H