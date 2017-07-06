#ifndef MESSAGE_H
#define MESSAGE_H

#include "exponentialbackoff.h"
#include "macros.h"

#include <iostream>
#include <cstdint>
#include <memory>

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

typedef std::string                     FcmMessageId_t;// fcmfieldnames::MESSAGE_ID field.
typedef std::string                     GroupId_t;
typedef std::int64_t                    SequenceId_t; // std::int64_t as string.
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
  static const char* const SEQUENCE_ID      = "sequence_id";
  static const char* const GROUP_ID         = "group_id";
  static const char* const SESSION_ID       = "session_id";
  static const char* const ERROR_DESC       = "error_description";
  static const char* const FCM_DATA         = "fcm_data";
}


/*!
 * \brief The MessageType enum
 * Gimmm subsystem message types.
 */
enum class MessageType: char
{
    UNKNOWN = 0,
    LOGON = 1,              // BAL --> GIMMM
    LOGON_RESPONSE,         // GIMMM --> BAL
    ACK = 2,                // BAL --> GIMMM
    UPSTREAM = 3,           // GIMMM --> BAL
    DOWNSTREAM = 4,         // BAL --> GIMMM
    DOWNSTREAM_ACK = 5,     // GIMMM --> BAL
    DOWNSTREAM_RECEIPT = 6, // GIMMM --> BAL
    DOWNSTREAM_REJECT = 7   // GIMMM --> BAL
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
    PENDING_ACK     = 2,
    DELIVERED       = 3,
    DELIVERY_FAILED = 4
};


class Message
{
        std::string         __enteredDatetime; //YYYY-MM-DD HH:MM:SS.SSS
        SequenceId_t        __sequenceId;
        SessionId_t         __sourceSessionId;
        SessionId_t         __targetSessionId;
        MessageType         __type;
        FcmMessageId_t      __fcmMessageId;
        GroupId_t           __groupId;
        MessageState        __state;
        std::string         __lastUpdateDatetime; //YYYY-MM-DD HH:MM:SS.SSS
        PayloadPtr_t        __payload;
        ExponentialBackoff  __exboff;

        int                 __nRetry;
        int                 __maxRetry;
        bool                __retryInProgress;
        friend std::ostream &operator<< (std::ostream&, const Message&);
    public:
        Message();
        Message(SequenceId_t seqid,
                MessageType type,
                const FcmMessageId_t& msgid,
                const GroupId_t& gid,
                const SessionId_t& source_sess_id,
                const SessionId_t& target_sess_id,
                PayloadPtr_t& payload,
                MessageState state = MessageState::NEW);
        Message(const Message& rhs);
        const Message& operator=(const Message& rhs);
        virtual ~Message();
        //setters
        void setEnteredDatetime(const std::string& datetime) {__enteredDatetime = datetime;}
        void setLastUpdateDatetime(const std::string& datetime) {__lastUpdateDatetime = datetime;}
        void setSequenceId(const SequenceId_t& sequence_id) { __sequenceId = sequence_id;}
        void setType(MessageType type){__type = type;}
        void setFcmMessageId(const FcmMessageId_t& mid) { __fcmMessageId = mid;}
        void setGroupId(const GroupId_t& gid) { __groupId = gid;}
        void setTargetSessionId(const SessionId_t& sid) { __targetSessionId = sid;}
        void setSourceSessionId(const SessionId_t& sid) { __sourceSessionId = sid;}
        void setState(MessageState state){__state = state;}
        void setPayload(PayloadPtr_t mptr) { __payload = mptr;}
        void setMaxRetry(int max_retry) { __maxRetry = max_retry;}
        void setRetryInProgress(bool val) { __retryInProgress = val;}

        //getters
        const std::string&  getEnteredDatetime()const { return __enteredDatetime;}
        const std::string&  getLastUpdateDatetime()const { return __lastUpdateDatetime;}
        SequenceId_t        getSequenceId() const { return __sequenceId;}
        MessageType         getType()const { return __type;}
        const FcmMessageId_t&  getFcmMessageId() const { return __fcmMessageId;}
        GroupId_t           getGroupId() const { return __groupId;}
        const SessionId_t&  getTargetSessionId()const { return __targetSessionId;}
        const SessionId_t&  getSourceSessionId()const { return __sourceSessionId;}
        MessageState        getState()const { return __state;}
        PayloadPtr_t        getPayload() const { return __payload;}
        int                 getMaxRetry() const { return __maxRetry;}
        bool                getRetryInProgress() const { return true;}

        int getNextRetryTimeout();
        std::string getMessageIdentifier()const;
    private:
};

inline std::ostream &operator<<( std::ostream& output, const Message& rhs)
{
      output << "Sequence ID:" << rhs.getSequenceId() << std::endl;
      output << "Source Session ID:" << rhs.getSourceSessionId() << std::endl;
      output << "Target Session ID:" << rhs.getTargetSessionId() << std::endl;
      output << "Type:" << (int)rhs.getType() << std::endl;
      output << "FCM Message ID:" << rhs.getFcmMessageId() << std::endl;
      output << "Group ID:" << rhs.getGroupId() << std::endl;
      output << "State:" << (int)rhs.getState() << std::endl;
      output << "Payload:" ;
      QJsonDocument& jdoc = *(rhs.getPayload());
      PRINT_JSON_DOC_RAW (output, jdoc);

      return output;
}


#endif // MESSAGE_H
