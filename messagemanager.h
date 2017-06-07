#ifndef MESSAGEMANAGER_H
#define MESSAGEMANAGER_H


#include "message.h"
#include "dbconnection.h"

#include <queue>
#include <set>

typedef std::map<SequenceId_t, MessagePtr_t>    MessageQueue_t;

/*!
 * \brief The Session class
 *        messages grp by session id so that we can re
 *        send all pending messages when a session reconnects.
 *        For downstream message, this is the fcm session.
 */
class Session
{
    SessionId_t         __sessionId;
    MessageQueue_t      __mainMsgQueue;//SequenceId_t, message.
    public:
        Session(SessionId_t sid):__sessionId(sid){;}
        void add(const MessagePtr_t& msg);
        void remove(const SequenceId_t& seqid);
};

typedef std::shared_ptr<Session> SessionPtr_t;

inline void Session::add(const MessagePtr_t &msg)
{
    SequenceId_t sid = msg->getSequenceId();
    __mainMsgQueue.emplace(sid, msg);
}

inline void Session::remove(const SequenceId_t &seqid)
{
    __mainMsgQueue.erase(seqid);
}


/*!
 * \brief The Group class
 */
class Group
{
        GroupId_t                               __groupId;
        MessageQueue_t                          __msgQueue;
    public:
        Group(const GroupId_t& gid):__groupId(gid){;}
        void add(const MessagePtr_t& msg);
        void remove(const SequenceId_t& msgid);
        bool canSend(const MessagePtr_t& msg);
};

typedef std::shared_ptr<Group> GroupPtr_t;

inline void Group::add(const MessagePtr_t &msg)
{
    SequenceId_t seqid = msg->getSequenceId();
    __msgQueue.emplace(seqid, msg);
}

inline void Group::remove(const SequenceId_t &sequence_id)
{
    __msgQueue.erase(sequence_id);
}

/*!
 * \brief Group::canSend
 * A message that is part of a group can be send if there are no
 * other messages ahead of it. Messages of the same group are push
 * into the internal map with the increasing sequence# that is assigned
 * when the message was recieved. This ensures that the earliest
 * message recieved is at the beginning of the map followed by the
 * rest.
 * \param msg Whether this 'msg' can be send now?
 * \return  true if it't the first element in the queue else false.
 */
inline bool Group::canSend(const MessagePtr_t &msg)
{
    const MessageId_t& mid = msg->getMessageId();
    auto it = __msgQueue.begin();
    auto front = it->second;
    if (mid == front->getMessageId())
        return true;
    else
        return false;
}


/*!
 * \brief The MessageManager class
 */
class MessageManager
{
    // # of messages pending ack from FCM
    std::uint64_t                           __maxPendingAllowed;
    std::uint64_t                           __pendingAckCount;
    //main queue that stores msg in order or reciept.
    std::map<MessageId_t, SequenceId_t>     __sequenceIdMap;//msgid --> SequenceId_t
    MessageQueue_t                          __messages;//SequenceId_t, message. //TODO check for order.
    // msgid --> group information .
    std::map<GroupId_t, GroupPtr_t>         __groups;
    // sessionid --> Session
    std::map<SessionId_t, SessionPtr_t>     __sessions;

    public:
        explicit MessageManager(std::int64_t maxpendingallowed = MAX_PENDING_MESSAGES);
        std::uint64_t       getPendingAckCount()const { return __pendingAckCount;}
        void                add(const MessagePtr_t& msg);
        void                updateState(const MessageId_t& msg, MessageState newstate);
        void                remove(const MessageId_t& msgid);
        int                 canSend(const MessagePtr_t& msg)const;
        int                 canSendOnReconnect(const MessagePtr_t& msg)const;
        MessagePtr_t        getNext()const;
        MessageQueue_t&     getMessages() { return __messages;}
        const MessagePtr_t  findMessage(MessageId_t msgid)const;
        bool                isMessagePending() const { return (__pendingAckCount > __maxPendingAllowed);}
        void                incrementPendingAckCount() { __pendingAckCount++;}

    private:
        void                addToMessages(const MessagePtr_t& msg);
        void                addToGroups(const MessagePtr_t& msg);
        void                addToSessions(const MessagePtr_t& msg);
        void                removeFromMessages(const MessageId_t& msgid, const SequenceId_t& seqid);
        void                removeFromGroups(const GroupId_t& gid, const SequenceId_t& seqid);
        void                removeFromSessions(const SessionId_t& sessid, const SequenceId_t& seqid);
        void                decrementPendingAckCount(){ __pendingAckCount--;}
        SequenceId_t        findSequenceId(const MessageId_t& msgid) const;
        GroupPtr_t          findGroup(const GroupId_t& gid)const;
        SessionPtr_t        findSession(const SessionId_t& sessid)const;
};

#endif // MESSAGEMANAGER_H
