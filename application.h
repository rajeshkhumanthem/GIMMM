#ifndef APPLICATION_H
#define APPLICATION_H

#include "fcmconnection.h"
#include "balsession.h"
#include "message.h"

#include <cstring>
#include <map>
#include <set>
#include <queue>

#include <QObject>
#include <QTcpServer>
#include <QSocketNotifier>


// Authenticated sessions. Key = category, Val = a BAL session.
typedef std::map<std::string, BALSessionPtr_t> SessionMap;
// Unauthenticated sessions. Key = socket descriptor, Val = a BALConn.
typedef std::map<qintptr,     BALConnPtr_t> SessionMapU;
typedef std::map<int, FcmConnectionPtr_t> FcmConnectionsMap;


/*!
 * \brief The Application class
 */
class Application:public QObject
{
        Q_OBJECT
        // BAL stuff
        QTcpServer                  __serverSocket;     // Socket for incomming BAL connection.
        quint16                     __serverPortNo;     // Port No for incomming BAL connection.
        QHostAddress                __serverHostAddress;// Host for incomming BAL connection.

        // Fcm stuff
        int                         __fcmConnCount;
        FcmConnectionsMap           __fcmConnectionsMap;
        QString                     __fcmServerId;      // FCM server id; read from config.ini
        QString                     __fcmServerKey;     // FCM server key; read from config.ini
        QString                     __fcmHostAddress;   // FCM host add; read from config.ini
        quint16                     __fcmPortNo;        // FCM port no; read from config.ini
        SessionMap                  __sessionMap;
        SessionMapU                 __sessionMapU;
        // messages already uploaded to FCM but awaiting 'ack'.
        std::map<std::string, DownstreamMessagePtr_t>   __downstreamMessages;
        // messages waiting to be uploaded to FCM because we have exceeded 100 un acked messages.
        std::queue<DownstreamMessagePtr_t>              __downstreamMessagesPending;
        // messages waiting to be forwarded to BAL.
        std::map<std::string, UpstreamMessagePtr_t>     __upstreamMessages;

        // Variables to help setup catchers for
        // SIGTERM & SIGHUP
        static int sighupFd[2];
        static int sigtermFd[2];
        QSocketNotifier *snHup;
        QSocketNotifier *snTerm;

    public:
        Application();
        ~Application();

        // POSIX signal handlers.
        static void hupSignalHandler(int unused);
        static void termSignalHandler(int unused);
    signals:
        void sendMessage(const QJsonDocument& data);
    public slots:
        // Qt signal handlers.
        void handleSigHup();
        void handleSigTerm();

        //control msg slots
        void handleNewBALConnection();
        void handleBALSocketReadyRead();
        void handleBALSocketDisconnected();
        void handleBALmsg(QTcpSocket* socket,
                          const QJsonDocument& json);
        // FCM handle slots
        void handleNewUpstreamMessage(int id, const QJsonDocument& json);
        void handleAckMessage(int id, const QJsonDocument& json);
        void handleNackMessage(int id, const QJsonDocument& json);
        void handleConnectionStarted(int id);
        void handleConnectionEstablished(int id);
        void handleConnectionShutdownStarted(int id);
        void handleConnectionShutdownCompleted(int id);
        void handleConnectionError(int id, const QString& error);
        void handleConnectionLost(int id);
        void handleXmppHandshakeStarted(int id);
        void handleSessionEstablished(int id);
        void handleStreamClosed(int id);
        void handleHeartbeatRecieved(int id);
        void handleFcmConnectionError(int id, const QString& err);
        void handleConnectionDrainingStarted(int id);
    private:
        // setup functions
        void start();
        FcmConnectionPtr_t createFcmHandle();
        void setupOsSignalCatcher();
        void setupTcpServer();
        void setupFcmHandle(FcmConnectionPtr_t fcmconn);
        void readConfigFile();
        // GCM URI
        void printProperties();
        void retryWithExponentialBackoff(std::string msg_id);
        void sendAckMessage(const QJsonDocument& original_msg);
        std::string getPeerDetail(const QTcpSocket* socket);
        void handleAuthenticationTimeout(BALConnPtr_t sess);
        void handleBALLogonRequest(QTcpSocket* socket,
                                   const std::string& session_id);
        void trySendingDownstreamMessage(const QJsonDocument& downstream_msg);
        void retrySendingDownstreamMessage(DownstreamMessagePtr_t ptr);
        void handleDownstreamUploadFailure(DownstreamMessagePtr_t ptr);

        void tryForwardingUpstreamMsg(const QJsonDocument& phantom_msg);
        void resendPendingUpstreamMessages();
        void handleBALAckMsg(const std::string& msgid);
        int  getNextFcmConnectionId(){ return ++__fcmConnCount;}
};

#endif // APPLICATION_H
