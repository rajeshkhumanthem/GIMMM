#include "message.h"

#include <sstream>

Message::Message()
    :__sequenceId(0),
     __type(MessageType::UNKNOWN),
     __state(MessageState::UNKNOWN),
     __retryInProgress(false)
{
}

/*!
 * \brief Message::createMessage
 * \param seqid
 * \param type
 * \param msgid
 * \param gid
 * \param source_sess_id
 * \param target_sess_id
 * \param payload
 * \param state
 * \return
 */
Message::Message(
        SequenceId_t seqid,
        MessageType type,
        const FcmMessageId_t &msgid,
        const GroupId_t &gid,
        const SessionId_t &source_sess_id,
        const SessionId_t &target_sess_id,
        PayloadPtr_t &payload,
        MessageState state)
    :__sequenceId(seqid),
     __sourceSessionId(source_sess_id),
     __targetSessionId(target_sess_id),
     __type(type),
     __fcmMessageId(msgid),
     __groupId(gid),
     __state(state),
     __payload(payload),
     __nRetry(0),
     __maxRetry(-1),
     __retryInProgress(false)
{
}

Message::~Message()
{
}

const Message& Message::operator =(const Message& rhs)
{
    if (&rhs != this)
    {
        this->__sequenceId          = rhs.__sequenceId;
        this->__type                = rhs.__type;
        this->__fcmMessageId        = rhs.__fcmMessageId;
        this->__groupId             = rhs.__groupId;
        this->__sourceSessionId     = rhs.__sourceSessionId;
        this->__targetSessionId     = rhs.__targetSessionId;
        this->__state               = rhs.__state;
        this->__exboff              = rhs.__exboff;

        QJsonDocument& p  = *(rhs.__payload);
        this->__payload.reset(new QJsonDocument(p));
    }
    return *this;
}


Message::Message(const Message &rhs)
{
    *this = rhs;
}


std::string Message::getMessageIdentifier()const
{
    std::stringstream id;
    id << "fcmmsgid: " << getFcmMessageId()
       << ", sequenceid: " << getSequenceId();
    return id.str();
}

int Message::getNextRetryTimeout() 
{
    __nRetry++;

    if ( __maxRetry != -1)
    {
        if ( __nRetry > __maxRetry)
            return -1;
    }
    return __exboff.next();
}
