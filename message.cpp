#include "message.h"

Message::Message()
    :__state(MessageState::NEW)
{
}

Message::~Message()
{

}

const Message& Message::operator =(const Message& rhs)
{
    if (&rhs != this)
    {
        this->__messageId           = rhs.__messageId;
        this->__groupId             = rhs.__groupId;
        this->__sequenceId          = rhs.__sequenceId;
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

/*!
 * \brief MessageManager::createMessage
 * \param type
 * \param msgid
 * \param gid
 * \param sess_id
 * \param payload
 * \param state
 * \return
 */
MessagePtr_t Message::createMessage(
        SequenceId_t seqid,
        MessageType type,
        const MessageId_t &msgid,
        const GroupId_t &gid,
        const SessionId_t &source_sess_id,
        const SessionId_t &target_sess_id,
        PayloadPtr_t &payload,
        MessageState state)
{
    MessagePtr_t ptr ( new Message());
    ptr->setType(type);
    ptr->setMessageId(msgid);
    ptr->setSequenceId(seqid);
    ptr->setGroupId(gid);
    ptr->setSourceSessionId(source_sess_id);
    ptr->setTargetSessionId(target_sess_id);
    ptr->setPayload(payload);
    ptr->setState(state);
    return ptr;
}
