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

/*!
 * \brief MessageId_t
 */
typedef std::uint64_t MessageId_t;

/*!
 * \brief FcmMsgPtr_t
 */
typedef std::shared_ptr<QJsonDocument> FcmMsgPtr_t;

/*!
 * \brief PhantomMsgPtr_t
 */
typedef std::shared_ptr<QJsonDocument> PhantomMsgPtr_t;


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
}


/*!
 *
 */
namespace phantomfieldnames
{
  static const char* const MESSAGE_TYPE     = "message_type";
  static const char* const MESSAGE_ID       = "message_id";
  static const char* const SESSION_ID       = "session_id";
  static const char* const ERROR            = "error";
  static const char* const ERROR_DESC       = "error_description";
  static const char* const FCM_DATA         = "fcm_data";
}


/*!
 * \brief The MessageType enum
 */
enum class MessageType: int
{
  UNKNOWN = 0,
  ACK = 1,
  DOWNSTREAM = 2,
};


/*!
 * \brief The MessageContentType enum
 */
enum class MessageContentType: int
{
    UNKNOWN         = 0,
    DATA            = 1, // Handled by client app. 4KB limit.
    NOTIFICATION    = 2  // Display message. 2KB limit.

};


/*!
 * \brief The Message class
 *        Convenient class to create 'Downstream XMPP message(JSON)'.
 *        Consists of 3 parts; Target, Options & Data.
 *        See https://firebase.google.com/docs/cloud-messaging/xmpp-server-ref for details.
 */
class Message
{
    MessageType         __messageType;
    MessageContentType  __messageContentType;
    std::string         __typeString;
    // Target
    std::string         __to;
    std::string         __condition;

    //Options
    std::string         __messageId;
    std::string         __collapseKey;
    std::string         __priority;
    QVariant            __contentAvailable;
    QVariant            __mutableContent;
    std::int32_t        __timeToLive;
    QVariant            __deliveryRecieptRequested;
    QVariant            __dryRun;

    //Payload
    QJsonObject         __data;

public:
    Message(MessageType type = MessageType::UNKNOWN);
    ~Message() {}
    QJsonDocument toJson(); // throw std::exception
    void setTo(const std::string& to) { __to = to;}
    void setTo(const char* to) { __to = to;}
    void setCondition(const std::string& condition){ __condition = condition;}
    void setMessageId(const std::string& message_id) { __messageId = message_id;}
    void setCollapseKey( const std::string& collapse_key) { __collapseKey = collapse_key;}
    void setpriority(const std::string& priority) { __priority = priority;}
    void setContentAvailable(bool content_available) { __contentAvailable = content_available;}
    void setTimeToLive (std::int32_t ttl) { __timeToLive = ttl;}
    void setDeliveryRecieptRequested(bool dcr) { __deliveryRecieptRequested = dcr;}
    void setDryRun (bool dry_run) { __dryRun = dry_run;}
    void setData(const QJsonObject& data) { __data = data;}
    /*!
     * \brief validate
     *        Call to validate the current message.
     *        Throws 'std::invalid_argument' exception on failure.
     */
    void validate()const;

    MessageType getMessageType() const { return __messageType;}
    MessageContentType getMessageContentType() { return __messageContentType;}
    const std::string& getTo()const { return __to;}
    const std::string& getCondition() const { return __condition;}
    const std::string& getMessageId() const { return __messageId;}

    // OPTIONAL FIELDS
    /*!
     * \brief getCollapseKey: Optional Field.
     * \return
     */
    const std::string& getCollapseKey() const { return __collapseKey;}
    /*!
     * \brief getPriority: Optional Field.
     * \return
     */
    const std::string& getPriority() const { return __priority;}
    /*!
     * \brief getTimeToLive: Optional Field.
     * \return
     */
    std::int32_t       getTimeToLive() const { return __timeToLive;}
    /*!
     * \brief getContentAvailable: Optional Field.
     *          Possible value is NULL/true/false.
     *          User should check for NULL before using it.
     * \return: Returns a QVariant which could be NULL since this is an optional field.
     */
    QVariant           getContentAvailable() const { return __contentAvailable;}
    /*!
     * \brief getMutableContent: Optional Field.
     *          Possible value is NULL/true/false.
     *          User should check for NULL before using it.
     * \return: Returns a QVariant which could be NULL since this is an optional field.
     */
    QVariant           getMutableContent() const { return __mutableContent;}
    /*!
     * \brief getDeliveryRecieptRequested: Optional Field.
     *          Possible value is NULL/true/false.
     *          User should check for NULL before using it.
     * \return: Returns a QVariant which could be NULL since this is an optional field.
     */
    QVariant           getDeliveryRecieptRequested() const { return __deliveryRecieptRequested;}
    /*!
     * \brief getDryRun: Optional Field.
     *          Possible value is NULL/true/false.
     *          User should check for NULL before using it.
     * \return: Returns a QVariant which could be NULL since this is an optional field.
     */
    QVariant           getDryRun()const { return __dryRun;}
private:
    QJsonDocument createAck();
    QJsonDocument createDownstream();
    void validateAck()const;
    void validateDownstream()const;
};


/*!
 * \brief The DownstreamMessage class
 */
class DownstreamMessage
{
        ExponentialBackoff __exboff;
        std::string __messageId;
        FcmMsgPtr_t __fcmmsgPtr;
    public:
        void setMessageId(const std::string& mid) { __messageId = mid;}
        void setMsg(FcmMsgPtr_t mptr) { __fcmmsgPtr = mptr;}

        const std::string& getMessageId() const { return __messageId;}
        FcmMsgPtr_t getMsg() { return __fcmmsgPtr;}

        DownstreamMessage():__exboff(0, MAX_DOWNSTREAM_UPLOAD_RETRY){;}
        int getNextRetryTimeout() { return __exboff.next();}
};

/*!
 * \brief DownstreamMessagePtr_t
 */
typedef std::shared_ptr<DownstreamMessage> DownstreamMessagePtr_t;


/*!
 * \brief The UpstreamMessage class
 */
class UpstreamMessage
{
        ExponentialBackoff __exboff;
        std::string __messageId;
        PhantomMsgPtr_t __msgPtr;
    public:
        void setMessageId(const std::string& mid) { __messageId = mid;}
        void setMsg(PhantomMsgPtr_t mptr) { __msgPtr = mptr;}

        const std::string& getMessageId() const { return __messageId;}
        PhantomMsgPtr_t getMsg() { return __msgPtr;}

        UpstreamMessage():__exboff(0, MAX_UPSTREAM_UPLOAD_RETRY){;}
        int getNextRetryTimeout() { return __exboff.next();}
};

/*!
 * \brief UpstreamMessagePtr_t
 */
typedef std::shared_ptr<UpstreamMessage> UpstreamMessagePtr_t;

#endif // MESSAGE_H
