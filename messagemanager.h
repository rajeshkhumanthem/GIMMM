#ifndef MESSAGEMANAGER_H
#define MESSAGEMANAGER_H


#include "message.h"
#include "dbconnection.h"

#include <queue>
#include <set>
#include <sstream>


typedef std::map<FcmMessageId_t, SequenceId_t>    SequenceIdMap_t;
typedef std::map<SequenceId_t, MessagePtr_t>    MessageQueue_t;

/*!
 * \brief The Group class
 */
class Group
{
        GroupId_t                               __groupId;
        MessageQueue_t                          __msgQueue;
    public:
        Group(const GroupId_t& gid)
            :__groupId(gid)
        {
            if (__groupId.empty())
            {
                std::string err = "Group id cannot be blank";
                THROW_INVALID_ARGUMENT_EXCEPTION (err);
            }
        }

        //getter
        GroupId_t getGroupId()const { return __groupId;}
        const MessageQueue_t&  getMessageQueue() const { return __msgQueue;}

        void add(const MessagePtr_t& msg);
        void remove(const SequenceId_t& msgid);
        bool canSend(const MessagePtr_t& msg);
};

typedef std::shared_ptr<Group> GroupPtr_t;
typedef std::map<GroupId_t, GroupPtr_t>         GroupMap_t;

inline void Group::add(const MessagePtr_t &msg)
{
    if ( msg->getGroupId() == __groupId)
    {
        SequenceId_t seqid = msg->getSequenceId();
        __msgQueue.emplace(seqid, msg);
    }
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
    const SequenceId_t& mid = msg->getSequenceId();
    auto it = __msgQueue.begin();
    auto front = it->second;
    if (mid == front->getSequenceId())
        return true;
    else
        return false;
}


/*!
 * \brief The MessageManager class
 * MessageManager holds all the upstream/downstream messages for each session until
 * an acknowledgement is recieved. It also arbiters whether a message can be send or
 * not based on certain rule viz max pending rule, group rule etc.
 */
class MessageManager
{
    std::string                             __sessionId;
    // # of messages pending ack from FCM
    std::uint64_t                           __maxPendingAllowed;
    std::uint64_t                           __pendingAckCount;
    //sequence id lookup map.
    SequenceIdMap_t                         __sequenceIdMap;//msgid --> SequenceId_t

    //main queue that stores msg in order or reciept.
    MessageQueue_t                          __messages;//SequenceId_t, message. //TODO check for order.
    GroupMap_t                              __groups;// msgid --> group information .

    public:
        MessageManager(const std::string& sessionid,
                       std::int64_t maxpendingallowed = MAX_PENDING_MESSAGES);

        // getters
        std::string             getSessionId() const { return __sessionId;}
        std::uint64_t           getMaxPendingAllowed()const { return __maxPendingAllowed;}
        std::uint64_t           getMaxPendingAckCount()const { return __pendingAckCount;}
        const SequenceIdMap_t&  getSequenceIdMap() const { return __sequenceIdMap;}
        std::uint64_t           getPendingAckCount()const { return __pendingAckCount;}
        MessageQueue_t&         getMessages() { return __messages;}
        const GroupMap_t&       getGroupsMap()const { return __groups;}



        void                addMessage(const SequenceId_t& seqid, const MessagePtr_t& msg);
        void                removeMessage(const SequenceId_t& seqid);
        const MessagePtr_t  findMessage(SequenceId_t seqid)const;
        MessagePtr_t        getNext()const;
        bool                isMessagePending() const { return (__pendingAckCount > __maxPendingAllowed);}
        void                incrementPendingAckCount() { __pendingAckCount++;}
        int                 canSendMessage(const MessagePtr_t& msg)const;
        int                 canSendMessageOnReconnect(const MessagePtr_t& msg)const;
        //convenience functions.
        const MessagePtr_t  findMessageWithFcmMsgId(FcmMessageId_t fcm_msgid)const;
        void                removeMessageWithFcmMsgId(FcmMessageId_t fcm_msgid);

    private:
        void                addToGroups(const MessagePtr_t& msg);
        void                decrementPendingAckCount(){ if (__pendingAckCount != 0) __pendingAckCount--;}
        GroupPtr_t          findGroup(const GroupId_t& gid)const;
        void                removeFromGroups(const GroupId_t& gid, const SequenceId_t& seqid);
        void                removeFromSessions(const SessionId_t& sessid, const SequenceId_t& seqid);
        SequenceId_t        findSequenceId(const FcmMessageId_t& msgid) const;
};

#endif // MESSAGEMANAGER_H
