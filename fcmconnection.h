#ifndef FCMCONNECTION_H
#define FCMCONNECTION_H

#include "exponentialbackoff.h"

#include <QObject>
#include <QSslSocket>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <vector>
#include <memory>


#define DEFAULT_NSPACE_URI		"jabber:client"
#define STREAM_NSPACE_URI 		"http://etherx.jabber.org/streams"
#define SASL_NSPACE_URI                 "urn:ietf:params:xml:ns:xmpp-sasl"
#define BIND_NSPACE_URI                 "urn:ietf:params:xml:ns:xmpp-bind"
#define SESSION_NSPACE_URI		"urn:ietf:params:xml:ns:xmpp-session"
#define GCM_NSPACE_URI			"google:mobile:data"

#define WAIT_TIME_BEFORE_DISCONNECT 50 //msec


/*!
 * \brief The FcmSessionState enum
 */
enum class FcmSessionState:char
{
    UNKNOWN = 'U',
    AUTHENTICATED = 'A'
};

/*!
 * \brief The FcmConnection class
 */
class FcmConnection:public QObject
{
        Q_OBJECT
    private:
        QSslSocket                  __fcmSocket;   // fcm facing socket
        QXmlStreamWriter            __fcmWriter;   // writes to __socket
        QXmlStreamReader            __fcmReader;   // read to __socket
        QString                     __fcmServerId; // FCM server id; read from config.ini
        QString                     __fcmServerKey;// FCM server key; read from config.ini
        QString                     __fcmHostAddress;
        quint16                     __fcmPortNo;   //
        std::vector<std::string>    __authMethodVect; //
        FcmSessionState             __state;
        ExponentialBackoff          __expBoff;
        bool                        __connectionDrainingInProgress;
        int                         __id;


    public:
        FcmConnection(int id);
        ~FcmConnection();
        void connectToFcm(QString server_id,
                          QString server_key,
                          QString host,
                          quint16 port_no);
        void setServerId(const QString& id) { __fcmServerId = id;}
        void setServerKey(const QString& key){ __fcmServerKey = key;}
        void setFcmHost(const QString& host) { __fcmHostAddress = host;}
        void setFcmPortNo(quint16 port_no) { __fcmPortNo = port_no;}
        FcmSessionState getState()const { return __state;}
        int             getId()const { return __id;}
    public slots:
        // slots
        void socketEncrypted();
        void handleReadyRead();
        void handleDisconnected();
        void handleSendMessage(const QJsonDocument& data);
    signals:
        void connectionStarted(int id);
        void connectionEstablished(int id);
        void connectionShutdownStarted(int id);
        void connectionShutdownCompleted(int id);
        void connectionLost(int id);
        void connectionError(int id, const QString& error);
        void connectionDrainingStarted(int id);
        void connectionDrainingCompleted(int id);
        void xmppHandshakeStarted(int id);
        void saslSucess(int id);
        void streamOpened(int id);
        void streamClosed(int id);
        void sessionEstablished(int id);
        void heartbeatRecieved(int id);

        void newMessage(int id, const QJsonDocument& msg);
        void newAckMessage(int id, const QJsonDocument& msg);
        void newNackMessage(int id, const QJsonDocument& msg);
        void newReceiptMessage(int id, const QJsonDocument& msg);
    private:
        void parseXml();
        void handleStartElement();
        void handleOtherElement();
        void sendAuthenticationInfo();
        void sendIQBind();
        void startNewStream();
        void readConsoleCmd();
        void handleEndOfStream();
        // STREAM URI
        void handleStartStream();
        void handleFeatures();
        // SASL URI
        void handleSaslSuccess();
        void handleSaslFailure();
        // JABBER:CLIENT URI
        void handleIq();
        void handleMessage();
        void readMechanisms();
        void readMechanism();
        void readJid();
        void readBind();
        void readSession();
        void readIQBindResult();
        void connectToFirebase();
        // FCM URI
        void handleFcmMessage(const QString& json_str);
        void handleControlMessage(const QJsonDocument& json);
        void handleReceiptMessage(const QJsonDocument& json);
};

/*!
 * \brief FcmConnectionPtr_t
 */
typedef std::shared_ptr<FcmConnection> FcmConnectionPtr_t;

#endif // FCMCONNECTION_H
