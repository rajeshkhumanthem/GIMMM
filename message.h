#ifndef MESSAGE_H
#define MESSAGE_H

#include "exponentialbackoff.h"

#include <iostream>
#include <cstdint>

#include <QJsonObject>
#include <QJsonDocument>
#include <QVariant>

#define MAX_TTL                         2419200 // Max time to live in secs(28 days)
#define MIN_TTL                         0
#define MAX_PENDING_MESSAGES            100
#define MAX_DOWNSTREAM_UPLOAD_RETRY     10
#define MAX_UPSTREAM_UPLOAD_RETRY       10
#define MAX_LOGON_MSG_WAIT_TIME         10000 // in msec

class Message;

typedef std::string                     MessageId_t;
typedef std::string                     GroupId_t;
typedef std::uint64_t                   SequenceId_t;
typedef std::string                     SessionId_t;
typedef std::shared_ptr<QJsonDocument>  PayloadPtr_t;
typedef std::shared_ptr<Message> MessagePtr_t;

/*!
 * field names in the root JSON message inside a xmpp stanza
 */
namespace fcmfieldnames
{
  static const char* const MESSAGE_TYPE     = "message_type";
  static const char* const MESSAGE_ID       = "message_id";
  static const char* const ERROR            = "error";
  static const char* const ERROR_DESC       = "error_description";
  static const char* const DATA             = "data";
  static const char* const FROM             = "from";
  static const char* const CATEGORY         = "category";
  static const char* const CONTROL_TYPE     = "control_type";
  static const char* const TO               = "to";
}


/*!
 *
 */
namespace gimmmfieldnames
{
  static const char* const MESSAGE_TYPE     = "message_type";
  static const char* const MESSAGE_ID       = "message_id";
  static const char* const GROUP_ID         = "group_id";
  static const char* const SESSION_ID       = "session_id";
  static const char* const ERROR_STRING     = "error_string";
  static const char* const FCM_DATA         = "fcm_data";
}


/*!
 * \brief The MessageType enum
 */
enum class MessageType: char
{
    UNKNOWN = 0,
    ACK = 1,              // BAL --> GIMMM
    UPSTREAM = 2,         // GIMMM --> BAL
    DOWNSTREAM = 3,       // BAL --> GIMMM
    DOWNSTREAM_ACK = 4,   // GIMMM --> BAL
    DOWNSTREAM_REJECT = 5 // GIMMM --> BAL
};


/*!
 * \brief The MessageContentType enum
 */
enum class MessageContentType: char
{
    UNKNOWN         = 0,
    DATA            = 1, // Handled by client app. 4KB limit.
    NOTIFICATION    = 2  // Display message. 2KB limit.

};

enum class MessageState: char
{
    UNKNOWN         = 0,
    NEW             = 1,
    NACK            = 2,
    PENDING_ACK     = 3,
    DELIVERED       = 4,
    DELIVERY_FAILED = 5
};


class Message
{
        MessageId_t         __messageId;
        GroupId_t           __groupId;
        SequenceId_t        __sequenceId;
        SessionId_t         __sourceSessionId;
        SessionId_t         __targetSessionId;
        MessageState        __state;
        MessageType         __type;
        ExponentialBackoff  __exboff;
        PayloadPtr_t        __payload;
    public:
        Message();
        Message(const Message& rhs);
        const Message& operator=(const Message& rhs);
        virtual ~Message();
        static MessagePtr_t createMessage(
                            SequenceId_t seqid,
                            MessageType type,
                            const MessageId_t& msgid,
                            const GroupId_t& gid,
                            const SessionId_t& sess_id,
                            PayloadPtr_t& payload,
                            MessageState state = MessageState::NEW);
        //setters
        void setMessageId(const MessageId_t& mid) { __messageId = mid;}
        void setGroupId(const GroupId_t& gid) { __groupId = gid;}
        void setTargetSessionId(const SessionId_t& sid) { __targetSessionId = sid;}
        void setSourceSessionId(const SessionId_t& sid) { __sourceSessionId = sid;}
        void setState(MessageState state){__state = state;}
        void setType(MessageType type){__type = type;}
        void setPayload(PayloadPtr_t mptr) { __payload = mptr;}
        void setSequenceId(const SequenceId_t& sequence_id) { __sequenceId = sequence_id;}

        //getters
        const MessageId_t&  getMessageId() const { return __messageId;}
        SequenceId_t        getSequenceId() const { return __sequenceId;}
        GroupId_t           getGroupId() const { return __groupId;}
        PayloadPtr_t        getPayload() { return __payload;}
        const SessionId_t&  getTargetSessionId()const { return __targetSessionId;}
        const SessionId_t&  getSourceSessionId()const { return __sourceSessionId;}
        MessageState        getState()const { return __state;}
        MessageType         getType()const { return __type;}
        int getNextRetryTimeout() { return __exboff.next();}
    private:
};


#endif // MESSAGE_H
