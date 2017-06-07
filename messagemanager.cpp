#include "messagemanager.h"
#include "macros.h"

#include <sstream>

/*!
 * \brief MessageManager::MessageManager
 */
MessageManager::MessageManager(std::int64_t max_pending_allowed)
    :__maxPendingAllowed(max_pending_allowed),
     __pendingAckCount(0)
{

}



/*!
 * \brief MessageManager::add
 * \param msg
 */
void MessageManager::add(const MessagePtr_t &msg)
{
    addToMessages(msg);
    addToGroups(msg);
    addToSessions(msg);
}


/*!
 * \brief MessageManager::addToMessages
 * \param msg
 */
void MessageManager::addToMessages(const MessagePtr_t& msg)
{
    SequenceId_t seqid = msg->getSequenceId();
    MessageId_t  msgid = msg->getMessageId();

    __sequenceIdMap.emplace(msgid, seqid);
    __messages.emplace(seqid, msg);
}


/*!
 * \brief MessageManager::addToGroups
 * \param msg
 */
void MessageManager::addToGroups(const MessagePtr_t& msg)
{
    GroupId_t    grpid = msg->getGroupId();

    if ( grpid.empty() != false)
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
 * \brief MessageManager::addToSessions
 * \param msg
 */
void MessageManager::addToSessions(const MessagePtr_t& msg)
{
    SessionId_t sessid = msg->getTargetSessionId();

    auto it = __sessions.find(sessid);
    if ( it != __sessions.end())
    {
        it->second->add(msg);
    }else
    {
        SessionPtr_t ptr(new Session(sessid));
        ptr->add(msg);
        __sessions.emplace(sessid, ptr);
    }
}


/*!
 * \brief MessageManager::remove
 * \param msgid
 */
void MessageManager::remove(const MessageId_t &msgid)
{
    MessagePtr_t msg = findMessage(msgid);

    SequenceId_t seqid = msg->getSequenceId();
    GroupId_t    grpid = msg->getGroupId();
    SessionId_t sessid = msg->getTargetSessionId();

    removeFromMessages(msgid, seqid);
    removeFromGroups(grpid, seqid);
    removeFromSessions(sessid, seqid);

    decrementPendingAckCount();
}


/*!
 * \brief MessageManager::removeFromMessages
 * \param msgid
 */
void MessageManager::removeFromMessages(
        const MessageId_t& msgid,
        const SequenceId_t& seqid)
{
    __sequenceIdMap.erase(msgid);
    __messages.erase(seqid);
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
}


/*!
 * \brief MessageManager::removeFromSessions
 * \param sessid
 * \param seqid
 */
void MessageManager::removeFromSessions(
        const SessionId_t &sessid,
        const SequenceId_t &seqid)
{
    auto sess = findSession(sessid);
    sess->remove(seqid);
}


/*!
 * \brief MessageManager::findMessage
 * \param msgid
 * \return
 */
const MessagePtr_t MessageManager::findMessage(MessageId_t msgid)const
{
    SequenceId_t seqid = findSequenceId(msgid);
    auto it = __messages.find(seqid);
    if ( it != __messages.end())
    {
        return it->second;
    }
    std::stringstream err;
    err << "Cannot find message for msgid[" << msgid << "]";
    THROW_INVALID_ARGUMENT_EXCEPTION(err.str());
}


/*!
 * \brief MessageManager::canSend
 * \param msg
 * \return 0 = success
 *         1 = bad state.
 *         2 = too many messages in pending ack.
 *         3 = there are message/messages from the same grp awaiting 'ack'.
 */
int MessageManager::canSend(const MessagePtr_t& msg)const
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


//we resend messages for which we haven't recieved an ack as well on
//reconnect.
int MessageManager::canSendOnReconnect(const MessagePtr_t& msg)const
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
        if (canSend(msg) == 0) return msg;
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
SequenceId_t MessageManager::findSequenceId(const MessageId_t& msgid)const
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


/*!
 * \brief MessageManager::findSession
 * \param sessid
 * \return
 */
SessionPtr_t MessageManager::findSession(const SessionId_t& sessid)const
{
    auto it = __sessions.find(sessid);
    if ( it != __sessions.end())
    {
        return it->second;
    }
    std::stringstream err;
    err << "Cannot find session with session id[" << sessid << "]";
    THROW_INVALID_ARGUMENT_EXCEPTION(err.str());
}



