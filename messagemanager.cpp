#include "messagemanager.h"
#include "macros.h"

#include <sstream>


/*!
 * \brief MessageManager::MessageManager
 */
MessageManager::MessageManager(const std::string& sessionid,
                               std::int64_t max_pending_allowed)
    :__sessionId(sessionid),
     __maxPendingAllowed(max_pending_allowed),
     __pendingAckCount(0)
{

}


/*!
 * \brief MessageManager::add
 * \param msg
 */
void MessageManager::addMessage(const SequenceId_t& seqid, const MessagePtr_t &msg)
{
    FcmMessageId_t  msgid = msg->getFcmMessageId();

    // create fcm message id to sequence id mapping.
    __sequenceIdMap.emplace(msgid, seqid);

    // add to messages
    __messages.emplace(seqid, msg);

    addToGroups(msg);
    //addToSessions(msg);
}


/*!
 * \brief MessageManager::addToGroups
 * \param msg
 */
void MessageManager::addToGroups(const MessagePtr_t& msg)
{
    GroupId_t    grpid = msg->getGroupId();
    if ( !grpid.empty())
    {
        auto grp = __groups.find(grpid);
        if ( grp != __groups.end())
        {
            grp->second->add(msg);
        }else
        {
            GroupPtr_t ptr(new Group(grpid));
            ptr->add(msg);
            __groups.emplace(grpid, ptr);
        }
    }
}


/*!
 * \brief MessageManager::remove
 * \param msgid
 */
void MessageManager::removeMessage(const SequenceId_t &seqid)
{
    MessagePtr_t msg = findMessage(seqid);

    FcmMessageId_t fcm_msgid = msg->getFcmMessageId();
    GroupId_t    grpid = msg->getGroupId();
    SessionId_t sessid = msg->getTargetSessionId();

    __messages.erase(seqid);
    removeFromGroups(grpid, seqid);
    //removeFromSessions(sessid, seqid);

    // erase msg ->seqid mapping.
    __sequenceIdMap.erase(fcm_msgid);

    if (msg->getState() == MessageState::PENDING_ACK)
        decrementPendingAckCount();
}


/*!
 * \brief MessageManager::removeFromGroups
 * \param gid
 * \param seqid
 */
void MessageManager::removeFromGroups(
        const GroupId_t &gid,
        const SequenceId_t &seqid)
{
    // group could be empty.
    if (gid.empty()) return;

    auto group = findGroup(gid);
    group->remove(seqid);

    if (group->getMessageQueue().empty())
        __groups.erase(gid);
}


/*!
 * \brief MessageManager::findMessage
 * \param seqid
 * \return
 */
const MessagePtr_t MessageManager::findMessage(SequenceId_t seqid)const
{
    auto it = __messages.find(seqid);
    if ( it != __messages.end())
    {
        return it->second;
    }
    std::stringstream err;
    err << "Cannot find message for seqid[" << seqid << "]";
    THROW_INVALID_ARGUMENT_EXCEPTION(err.str());
}


/*!
 * \brief MessageManager::canSendMessage
 * \param msg
 * \return 0 = success
 *         1 = bad state.
 *         2 = too many messages in pending ack.
 *         3 = there are message/messages from the same grp awaiting 'ack'.
 */
int MessageManager::canSendMessage(const MessagePtr_t& msg)const
{
    // make sure msg is in the correct state.
    if ( msg->getState() != MessageState::NEW)
    {
        return 1;
    }
    // pending count rule.
    if ( getPendingAckCount() >= MAX_PENDING_MESSAGES)
    {
        return 2;
    }
    // group rule.
    auto gid = msg->getGroupId();
    if (!gid.empty())
    {
        auto grp = findGroup(gid);
        if (grp->canSend(msg) == false)
        {
            return 3;
        }
    }
    return 0;
}


/*!
 * \brief MessageManager::canSendMessageOnReconnect
 * \param msg
 *  Resend all messages that are in 'NEW' or 'PENDING_ACK'
 * \return
 *  0: Success
 *  1: Bad state
 *  2: Max pending message breached.
 *  3: There is another message of the same group ahead of 'msg'.
 */
int MessageManager::canSendMessageOnReconnect(const MessagePtr_t& msg)const
{
    // make sure msg is in the correct state.
    if ( msg->getState() != MessageState::NEW &&
         msg->getState() != MessageState::PENDING_ACK)
    {
        return 1;
    }
    // pending count rule.
    if ( getPendingAckCount() >= MAX_PENDING_MESSAGES)
    {
        return 2;
    }
    // group rule.
    auto gid = msg->getGroupId();
    if (!gid.empty())
    {
        auto grp = findGroup(gid);
        if (grp->canSend(msg) == false)
        {
            return 3;
        }
    }
    return 0;
}


/*!
 * \brief MessageManager::getNext
 * \return
 */
MessagePtr_t MessageManager::getNext()const
{
    for ( auto i: __messages)
    {
        MessagePtr_t& msg = i.second;
        if (canSendMessage(msg) == 0) return msg;
    }
    //nothing left to send, return null msg
    MessagePtr_t nullmsg;
    return nullmsg;
}


/*!
 * \brief MessageManager::findGroup
 * \param gid
 * \return
 */
GroupPtr_t MessageManager::findGroup(const GroupId_t& gid) const
{
    auto it = __groups.find(gid);
    if (it != __groups.end())
    {
        return it->second;
    }
    std::stringstream err;
    err << "Cannot find groupd with group id [" << gid << "]";
    THROW_INVALID_ARGUMENT_EXCEPTION(err.str());
}


/*!
 * \brief MessageManager::findSequenceId
 * \param msgid
 * \return
 */
SequenceId_t MessageManager::findSequenceId(const FcmMessageId_t& msgid)const
{
    auto it = __sequenceIdMap.find(msgid);
    if ( it != __sequenceIdMap.end())
    {
        return it->second;
    }
    std::stringstream err;
    err << "Cannot find sequenceid for msgid[" << msgid << "]";
    THROW_INVALID_ARGUMENT_EXCEPTION(err.str());
}


const MessagePtr_t MessageManager::findMessageWithFcmMsgId(FcmMessageId_t fcm_msgid)const
{
    SequenceId_t seqid = findSequenceId(fcm_msgid);
    MessagePtr_t msg = findMessage(seqid);
    return msg;
}

void MessageManager::removeMessageWithFcmMsgId(FcmMessageId_t fcm_msgid)
{
    SequenceId_t seqid = findSequenceId(fcm_msgid);
    removeMessage(seqid);
}


