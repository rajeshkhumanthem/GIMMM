#ifndef APPLICATION_H
#define APPLICATION_H

#include "fcmconnection.h"
#include "balsession.h"
#include "message.h"
#include "messagemanager.h"
#include "dbconnection.h"

#include <cstring>
#include <map>
#include <set>
#include <queue>

#include <QObject>
#include <QTcpServer>
#include <QSocketNotifier>


// Authenticated sessions. Key = category, Val = a BAL session.
typedef std::map<std::string, BALSessionPtr_t>  BalSessionMap_t;
// Unauthenticated sessions. Key = socket descriptor, Val = a BALConn.
typedef std::map<qintptr,     BALConnPtr_t>     SessionMapU;
typedef std::map<int, FcmConnectionPtr_t>       FcmConnectionsMap;


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
        MessageManager              __fcmMsgManager;

        BalSessionMap_t             __balSessionMap;    // sessionid --> Authenticated BAL map.
        SessionMapU                 __balSessionMapU;   // socket --> Unauthenticated BAL map.
        DbConnection                __dbConn;           // database connection handle.

        // Variables to help setup catchers for
        // SIGTERM & SIGHUP
        static int sigintFd[2];
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
        void handleSigInt();
        void handleSigTerm();

        //control msg slots
        void handleNewBALConnection();
        void handleBALSocketReadyRead();
        void handleBALSocketDisconnected();
        void handleBALmsg(QTcpSocket* socket,
                          const QJsonDocument& json);
        // FCM handle slots
        void handleFcmNewUpstreamMessage(int id, const QJsonDocument& json);
        void handleFcmAckMessage(int id, const QJsonDocument& json);
        void handleFcmNackMessage(int id, const QJsonDocument& json);
        void handleFcmReceiptMessage(int id, const QJsonDocument& json);
        void handleFcmConnectionStarted(int id);
        void handleFcmConnectionEstablished(int id);
        void handleFcmConnectionShutdownStarted(int id);
        void handleFcmConnectionShutdownCompleted(int id);
        void handleFcmConnectionError(int id, const QString& error);
        void handleFcmConnectionLost(int id);
        void handleFcmXmppHandshakeStarted(int id);
        void handleFcmSessionEstablished(int id);
        void handleFcmStreamClosed(int id);
        void handleFcmHeartbeatRecieved(int id);
        void handleFcmConnectionDrainingStarted(int id);
    private:
        // FCM downstream stuff
        void sendFcmAckMessage(const QJsonDocument& original_msg);
        void sendNextPendingDownstreamMessage(const MessageManager& msgmanager);
        void resendAllPendingDownstreamMessages();

        //BAL
        void handleBalAuthenticationTimeout(BALConnPtr_t sess);
        void handleBalLogonRequest(QTcpSocket* socket,
                                   const std::string& session_id);
        void handleBalDownstreamUploadRequest(const SessionId_t& sesion_id,
                                     const QJsonDocument& downstream_msg);
        void notifyDownstreamUploadFailure(const MessagePtr_t& ptr);
        void handleBalAckMsg(const SessionId_t& session_id,
                             const SequenceId_t& seqid);

        // helpers
        void start();
        void setupOsSignalCatcher();
        void readConfigFile();
        void setupTcpServer();

        FcmConnectionPtr_t createFcmHandle();
        void setupFcmHandle(FcmConnectionPtr_t fcmconn);
        void uploadToFcm(MessagePtr_t& msg);
        int  getNextFcmConnectionId(){ return ++__fcmConnCount;}
        void retryDownstreamWithExponentialBackoff(MessagePtr_t& msg);
        void resendPendingUpstreamMessages(const BALSessionPtr_t& sess);
        void forwardMsgToBalsession(const std::string& session_id,
                                    const MessagePtr_t& msg);
        void forwardMsg(const std::string& session_id, const MessagePtr_t& msg);

        void retryUpstreamWithExponentialBackoff(MessagePtr_t& msg);

        BALSessionPtr_t findBalSession(const SessionId_t& session_id);
        MessageManager& findBalMessageManager(const SessionId_t& bal_session_id);
        void            sendNextPendingUpstreamMessage(const MessageManager& msgmanager);
        std::string     getPeerDetail(const QTcpSocket* socket);
        void            printProperties();
};

#endif // APPLICATION_H
