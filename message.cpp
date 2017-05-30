#include "message.h"
#include "macros.h"

#include <sstream>


/*!
 * \brief Message::Message
 * \param type
 */
Message::Message(MessageType type)
    :__messageType(type), __timeToLive(10)
{
    if (type == MessageType::ACK)
        __typeString = "ack";
}


/*!
 * \brief Message::toJson
 * \return
 */
QJsonDocument Message::toJson()
{
    switch(__messageType)
    {
        case MessageType::ACK:
        {
            return createAck();
            break;
        }
        case MessageType::DOWNSTREAM:
        {
            return createDownstream();
            break;
        }
        default:
        {
            std::stringstream err;
            err << "Unknown message type<" << (int)__messageType << ">";
            THROW_INVALID_ARGUMENT_EXCEPTION(err.str());
        }
    }
}


/*!
 * \brief Message::createAck
 * \return
 */
QJsonDocument Message::createAck()
{
    QJsonDocument ackmsg;
    QJsonObject root;
    root["to"] = __to.c_str();
    root["message_id"] = __messageId.c_str();
    root["message_type"] = __typeString.c_str();
    ackmsg.setObject(root);
    return ackmsg;
}


/*!
 * \brief Message::createDownstream
 * \return
 */
QJsonDocument Message::createDownstream()
{
    QJsonDocument msg;
    QJsonObject root;
    root["to"] = __to.c_str();
    root["message_id"] = __messageId.c_str();
    root["time_to_live"] = __timeToLive;

    if (!__collapseKey.empty())
        root["collapse_key"]= __collapseKey.c_str();
    if (!__priority.empty())
        root["priority"] = __priority.c_str();

    if (!__contentAvailable.isNull())
        root["content_available"] = __contentAvailable.toBool();
    if (!__mutableContent.isNull())
        root["mutable_content"] = __mutableContent.toBool();
    if (!__deliveryRecieptRequested.isNull())
        root["deliver_reciept_requested"] = __deliveryRecieptRequested.toBool();
    if (!__dryRun.isNull())
        root["dry_run"] = __dryRun.toBool();

    root["data"] = __data;

    msg.setObject(root);
    return msg;
}


/*!
 * \brief Message::validate
 */
void Message::validate()const
{
    switch(__messageType)
    {
        case MessageType::ACK:
        {
            validateAck();
            break;
        }
        case MessageType::DOWNSTREAM:
        {
            validateDownstream();
            break;
        }
        default:
        {
            std::stringstream err;
            err << "Unknown message type<" << (int)__messageType << ">";
            THROW_INVALID_ARGUMENT_EXCEPTION(err.str());
        }
    }
}


/*!
 * \brief Message::validateAck
 */
void Message::validateAck() const
{
    if (__to.empty())
    {
        std::stringstream err;
        err << "Invalid ACK message. 'to' field cannot be empty.";
        THROW_INVALID_ARGUMENT_EXCEPTION(err.str());
    }
    if (__messageId.empty())
    {
        std::stringstream err;
        err << "Invalid ACK message. 'message_id' field cannot be empty.";
        THROW_INVALID_ARGUMENT_EXCEPTION(err.str());
    }
    if (__typeString != "ack")
    {
        std::stringstream err;
        err << "Invalid ACK message. 'message type' field is not set tp 'ack'.";
        THROW_INVALID_ARGUMENT_EXCEPTION(err.str());
    }
}


/*!
 * \brief Message::validateDownstream
 */
void Message::validateDownstream() const
{
  std::cout << "Validate downstream mesage" << std::endl;
    if (__to.empty() && __condition.empty())
    {
        std::stringstream err;
        err << "Invalid DOWNSTREAM message. Both 'to' && 'condition' field cannot be empty.";
        THROW_INVALID_ARGUMENT_EXCEPTION(err.str());
    }
    if (__messageId.empty() == true)
    {
        std::stringstream err;
        err << "Invalid DOWNSTREAM message. 'message_id' field cannot be empty.";
        THROW_INVALID_ARGUMENT_EXCEPTION(err.str());
    }
    // max is 4 week as per FCM documentation.
    if (__timeToLive < MIN_TTL || __timeToLive > MAX_TTL )
    {
        std::stringstream err;
        err << "Invalid DOWNSTREAM message. Invalid TTL <" << __timeToLive << "> found.";
        THROW_INVALID_ARGUMENT_EXCEPTION(err.str());
    }
}

