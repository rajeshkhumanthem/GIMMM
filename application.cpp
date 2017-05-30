#include "application.h"
#include "message.h"
#include "macros.h"
#include "balsession.h"

#include <QByteArray>
#include <QTimer>
#include <QDataStream>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QTcpServer>
#include <QAbstractSocket>

#include <iostream>
#include <cstdio>
#include <sstream>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>


int Application::sighupFd[2];
int Application::sigtermFd[2];


/*!
 * \brief Application::Application
 */
Application::Application()
    :__fcmConnCount(0)
{
    start();
}


/*!
 * \brief Application::~Application
 */
Application::~Application()
{
    //std::cout << "Closing connection to FCM.." << std::endl;
    //std::cout << "Shutting down...GOODBYE." << std::endl;
    __serverSocket.close();
}


/*!
 * \brief Application::start
 */
void Application::start()
{
    readConfigFile();
    setupOsSignalCatcher();
    setupTcpServer();

    FcmConnectionPtr_t fcmConn = createFcmHandle();
    setupFcmHandle(fcmConn);
    fcmConn->connectToFcm(__fcmServerId, __fcmServerKey, __fcmHostAddress, __fcmPortNo);
}


/*!
 * \brief Application::createFcmHandle
 * \return
 */
FcmConnectionPtr_t Application::createFcmHandle()
{
    int i = getNextFcmConnectionId();
    FcmConnectionPtr_t fcmConn(new FcmConnection(i));
    __fcmConnectionsMap.emplace(i, fcmConn);
    return fcmConn;
}


/*!
 * \brief Application::readConfigFile
 */
void Application::readConfigFile()
{
    std::cout << "Reading config file..." << std::endl;
    QSettings ini("./config.ini", QSettings::IniFormat);

    // FCM SECTION
    __fcmServerId  = ini.value("FCM_SECTION/server_id", "NULL").toString();
    if ( __fcmServerId == "NULL")
    {
        std::cout << "ERROR: Missing config parameter 'FCM_SECTION/server_id. Exiting." << std::endl;
        exit(0);
    }

    __fcmServerKey = ini.value("FCM_SECTION/server_key", "NULL").toString();
    if ( __fcmServerKey == "NULL")
    {
        std::cout << "ERROR: Missing config parameter 'FCM_SECTION/server_key. Exiting..." << std::endl;
        exit(0);
    }

    __fcmPortNo = ini.value("FCM_SECTION/port_no", 0).toInt();
    if (__fcmPortNo == 0)
    {
        std::cout << "ERROR: Missing config parameter 'FCM_SECTION/port_no. Exiting..." << std::endl;
        exit(0);
    }

    __fcmHostAddress = ini.value("FCM_SECTION/host_address", "NULL").toString();
    if ( __fcmHostAddress == "NULL")
    {
        std::cout << "ERROR: Missing config parameter 'FCM_SECTION/host_address. Exiting..." << std::endl;
        exit(0);
    }

    // SERVER SECTION
    __serverPortNo = ini.value("SERVER_SECTION/port_no", 0).toInt();
    if ( __serverPortNo == 0)
    {
        std::cout << "ERROR:Missing config parameter 'SERVER_SECTION/port_no. Exiting..." << std::endl;
        exit(0);
    }

    QString addr = ini.value("SERVER_SECTION/host_address", "NULL").toString();
    if ( addr == "NULL")
    {
        std::cout << "ERROR:Missing config parameter 'SERVER_SECTION/host_address. Exiting..." << std::endl;
        exit(0);
    }
    if (!__serverHostAddress.setAddress(addr))
    {
        std::cout << "ERROR: Unable to parse SERVER_SECTION/host_address. Exiting..." << std::endl;
        exit(0);
    }

    // BAL session SECTION
    QString balclient = ini.value("BAL_SECTION/session_id", "NULL").toString();
    if ( balclient == "NULL")
    {
        std::cout<< "ERROR: No BAL client session found. Exiting..." << std::endl;
        exit(0);
    }
    BALSessionPtr_t sess(new BALSession());
    sess->setSessionId(balclient.toStdString());
    __sessionMap.emplace(balclient.toStdString(), sess);
    printProperties();
}


/*!
 * \brief Application::setupOsSignalCatcher
 * Let's catch SIGTERM and SIGHUP so that if the server was brought down
 * via 'kill', it can still send the end of stream to FCM and cleanly shut
 * itself down.
 */
void Application::setupOsSignalCatcher()
{
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sighupFd))
        qFatal("Couldn't create HUP socketpair");

    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sigtermFd))
        qFatal("Couldn't create TERM socketpair");

    snHup = new QSocketNotifier(sighupFd[1], QSocketNotifier::Read, this);
    connect(snHup, SIGNAL(activated(int)), this, SLOT(handleSigHup()));
    snTerm = new QSocketNotifier(sigtermFd[1], QSocketNotifier::Read, this);
    connect(snTerm, SIGNAL(activated(int)), this, SLOT(handleSigTerm()));
}


/*!
 * \brief Application::setupTcpServer
 */
void Application::setupTcpServer()
{
    connect(&__serverSocket, &QTcpServer::newConnection,
            this, &Application::handleNewBALConnection);
    // Start listenning for Phantom Client connection request.
    if (__serverSocket.listen(__serverHostAddress, __serverPortNo))
    {
        std::cout << "Listenning at port 5000 for control cmd messages..." << std::endl;
    }
    else
    {
        std::cout << "Unable to listen @host:" << __serverHostAddress.toString().toStdString()
                  << ", @port:" << __serverPortNo << ".Exiting...GOODBYE!" << std::endl;
        exit(0);
    }
}


/*!
 * \brief Application::setupFcmStuff
 */
void Application::setupFcmHandle(FcmConnectionPtr_t fcmConnPtr)
{
    FcmConnection& fcmConn = *fcmConnPtr;
    // setup signal handlers
    connect(&fcmConn, SIGNAL(connectionStarted(int)),                   this, SLOT(handleConnectionStarted(int)));
    connect(&fcmConn, SIGNAL(connectionEstablished(int)),               this, SLOT(handleConnectionEstablished(int)));
    connect(&fcmConn, SIGNAL(connectionShutdownStarted(int)),           this, SLOT(handleConnectionShutdownStarted(int)));
    connect(&fcmConn, SIGNAL(connectionShutdownCompleted(int)),         this, SLOT(handleConnectionShutdownCompleted(int)));
    connect(&fcmConn, SIGNAL(connectionLost(int)),                      this, SLOT(handleConnectionLost(int)));
    connect(&fcmConn, SIGNAL(connectionDrainingStarted(int)),           this, SLOT(handleConnectionDrainingStarted(int)));
    connect(&fcmConn, SIGNAL(xmppHandshakeStarted(int)),                this, SLOT(handleXmppHandshakeStarted(int)));
    connect(&fcmConn, SIGNAL(sessionEstablished(int)),                  this, SLOT(handleSessionEstablished(int)));
    connect(&fcmConn, SIGNAL(streamClosed(int)),                        this, SLOT(handleStreamClosed(int)));
    connect(&fcmConn, SIGNAL(heartbeatRecieved(int)),                   this, SLOT(handleHeartbeatRecieved(int)));
    connect(&fcmConn, SIGNAL(connectionError(int, const QString&)),     this, SLOT(handleConnectionError(int, const QString&)));
    connect(&fcmConn, SIGNAL(fcmConnectionError(int, const QString&)),  this, SLOT(handleFcmConnectionError(int, const QString&)));
    connect(&fcmConn, SIGNAL(newMessage(int, const QJsonDocument&)),    this, SLOT(handleNewUpstreamMessage(int, const QJsonDocument&)));
    connect(&fcmConn, SIGNAL(newAckMessage(int, const QJsonDocument&)), this, SLOT(handleAckMessage(int, const QJsonDocument&)));
    connect(&fcmConn, SIGNAL(newNackMessage(int, const QJsonDocument&)),this, SLOT(handleNackMessage(int, const QJsonDocument&)));


    //setup queued connection to fcm connection handle for sending downstream message to FCM.
    connect(this, SIGNAL(sendMessage(const QJsonDocument&)),&fcmConn, SLOT(handleSendMessage(const QJsonDocument&)));
}


/*!
 * \brief Application::handleStreamClosed
 */
void Application::handleStreamClosed(int id)
{
    std::cout << FCM_TAG(id) <<"Recieved stream closed from FCM." << std::endl;
}


/*!
 * \brief Application::handleHeartbeatRecieved
 */
void Application::handleHeartbeatRecieved(int id)
{
    std::cout <<  FCM_TAG(id) << "Recieved keepalive from FCM" << std::endl;
}


void Application::handleFcmConnectionError(int id, const QString& err)
{
    std::cout << FCM_TAG(id) << "ERROR. Error string[" << err.toStdString() << "]" << std::endl;
}


/*!
 * \brief Application::handleConnectionStarted
 */
void Application::handleConnectionStarted(int id)
{
    std::cout << FCM_TAG(id) << "Connecting to FCM server..." << std::endl;
}


/*!
 * \brief Application::handleConnectionEstablished
 */
void Application::handleConnectionEstablished(int id)
{
    std::cout << FCM_TAG(id) << "Secure TLS channel established with FCM server..." << std::endl;
}


/*!
 * \brief Application::handleXmppHandshakeStarted
 */
void Application::handleXmppHandshakeStarted(int id)
{
    std::cout << FCM_TAG(id) <<"Starting XMPP handshake. Opening stream..." << std::endl;
}


/*!
 * \brief Application::handleSessionEstablished
 */
void Application::handleSessionEstablished(int id)
{
    std::cout << FCM_TAG(id) << "Session authenticated sucessfully.." << std::endl;
    std::cout << "----------------------------------------------------------------------------------------------------\n";
    std::cout << "-     SESSION WITH FCM ESTABLISHED SUCCESSFULLY        -" << std::endl;
    std::cout << "-     SESSION ID: " << __fcmServerId.toStdString() <<"\n";
    std::cout << "----------------------------------------------------------------------------------------------------" << std::endl;
}


/*!
 * \brief Application::handleConnectionLost
 */
void Application::handleConnectionLost(int id)
{
    std::cout << FCM_TAG(id) << "Disconnected to FCM server." << std::endl;
}


/*!
 * \brief Application::handleConnectionShutdownStarted
 */
void Application::handleConnectionShutdownStarted(int id)
{
    std::cout << FCM_TAG(id) <<"Shuting down connection to FCM..." << std::endl;
}


/*!
 * \brief Application::handleConnectionShutdownCompleted
 */
void Application::handleConnectionShutdownCompleted(int id)
{
    std::cout << FCM_TAG(id) << "Connection to FCM shutdown successfully." << std::endl;
}


/*!
 * \brief Application::handleConnectionError
 * \param error
 */
void Application::handleConnectionError(int id, const QString& error)
{
    std::cout << FCM_TAG(id) << "ERROR: Ssl errors:" << error.toStdString() << std::endl;
}


/*!
 * \brief Application::handleConnectionDrainingStarted
 * \param id
 */
void Application::handleConnectionDrainingStarted(int id)
{
    std::cout << FCM_TAG(id) << "Connection draining started..." << std::endl;

    // disconnect 'sendMessage()' connection to old fcm handle.
    auto lastFcmHandleIter = __fcmConnectionsMap.rbegin();
    FcmConnection& lastFcmHandle = *(lastFcmHandleIter->second);
    disconnect(this, 0, &lastFcmHandle, 0);

    std::cout << "Creating a new connection to FCM..." << std::endl;
    FcmConnectionPtr_t fcmConn = createFcmHandle();
    setupFcmHandle(fcmConn);
    fcmConn->connectToFcm(__fcmServerId, __fcmServerKey, __fcmHostAddress, __fcmPortNo);
}


/*!
 * \brief Application::handleNewUpstreamMessage
 * \param client_msg
 */
void Application::handleNewUpstreamMessage(int id, const QJsonDocument& client_msg)
{
    std::cout << FCM_TAG(id) << "[" << client_msg.toJson().toStdString() << "]" << std::endl;
    std::string from = client_msg.object().value(fcmfieldnames::FROM).toString().toStdString();
    std::string application_name = client_msg.object().value(fcmfieldnames::CATEGORY).toString().toStdString();
    std::string mid = client_msg.object().value(fcmfieldnames::MESSAGE_ID).toString().toStdString();

    std::cout << "Recieved 'upstream' message with msg id [" << mid<< "] from:" << from << std::endl;
    std::cout << "Application Name:" << application_name  << std::endl;

    sendAckMessage(client_msg);

    // add phantom header before sending to BAL.
    QJsonDocument phantom_msg;
    QJsonObject root;
    root[phantomfieldnames::MESSAGE_TYPE] = "UPSTREAM",
    root[phantomfieldnames::SESSION_ID]   =  application_name.c_str();
    root[phantomfieldnames::FCM_DATA] = client_msg.object();
    phantom_msg.setObject(root);

    PhantomMsgPtr_t pmsg(new QJsonDocument(client_msg));

    UpstreamMessagePtr_t msgptr;
    msgptr->setMessageId(mid);
    msgptr->setMsg(pmsg);

    __upstreamMessages.emplace(mid, msgptr);
    tryForwardingUpstreamMsg(phantom_msg);
}


/*!
 * \brief Application::tryForwardingUpstreamMsg
 * \param phantom_msg
 */
void Application::tryForwardingUpstreamMsg(const QJsonDocument& phantom_msg)
{
    QJsonObject fcm_data = phantom_msg.object().value(phantomfieldnames::FCM_DATA).toObject();
    std::string session_id = phantom_msg.object().value(phantomfieldnames::SESSION_ID).toString().toStdString();

    auto it = __sessionMap.find(session_id);
    if (it == __sessionMap.end())
    {
         std::cout << "ERROR:Recieved message for unknown BAL["
                   << session_id << "], did u forget to add it to the 'config' file."<< std::endl;
         return;
    }

    BALSessionPtr_t sess = it->second;
    bool status = sess->writeMessage(phantom_msg);
    if ( status == false)
    {
        std::cout << "ERROR: Forwarding upstream message to session[" << session_id << "]failed."
                  << "Will sync when session reconnects." << std::endl;
        sess->disconnectFromHost();
    }
}


/*!
 * \brief Application::retryWithExponentialBackoff
 * \param msg_id
 */
void Application::retryWithExponentialBackoff(std::string msg_id)
{
    auto it = __downstreamMessages.find(msg_id);
    if ( it != __downstreamMessages.end())
    {
        DownstreamMessagePtr_t ptr = it->second;
        int msec = ptr->getNextRetryTimeout();
        if ( msec != -1)
        {
            auto timerCallback = [ptr, this]{
                retrySendingDownstreamMessage(ptr);
            };
            // retry sending after 'msec' millisecs.
            QTimer::singleShot(msec, Qt::TimerType::PreciseTimer, timerCallback);
        }
        else
        {
            handleDownstreamUploadFailure(ptr);
        }
     }
}


/*!
 * \brief Application::retrySendingDownstreamMessage
 * \param ptr
 */
void Application::retrySendingDownstreamMessage(DownstreamMessagePtr_t ptr)
{
    QJsonDocument& jdoc = *(ptr->getMsg());
    emit sendMessage(jdoc);
}


/*!
 * \brief Application::handleDownstreamUploadFailure
 * \param ptr
 */
void Application::handleDownstreamUploadFailure(DownstreamMessagePtr_t ptr)
{
    std::cout << "ERROR: Droping downstream message\n.["
              << ptr->getMsg()->toJson().toStdString()
              << "], Max retry [" << MAX_DOWNSTREAM_UPLOAD_RETRY <<"] reached." << std::endl;
}


/*!
 * \brief Application::printProperties
 */
void Application::printProperties()
{
    std::cout << "FCM_SECTION/port_no:"         << __fcmPortNo << std::endl;
    std::cout << "FCM_SECTION/host_address:"    << __fcmHostAddress.toStdString() << std::endl;
    std::cout << "FCM_SECTION/server_id:"       << __fcmServerId.toStdString() << std::endl;
    std::cout << "FCM_SECTION/server_key:"      << __fcmServerKey.toStdString() << std::endl;
    std::cout << "SERVER_SECTION/port_no:"      << __serverPortNo << std::endl;
    std::cout << "SERVER_SECTION/host_address:" << __serverHostAddress.toString().toStdString() << std::endl;
    std::cout << "BAL_SECTION/sessions: TODO" << std::endl;
}


/*!
 * \brief Application::sendAckMessage
 * @https://firebase.google.com/docs/cloud-messaging/server
 * For each device message your app server receives from CCS,
 * it needs to send an ACK message. It never needs to send a NACK message.
 * If you don't send an ACK for a message, CCS resends it the next time
 * a new XMPP connection is established, unless the message expires first.
 * \param json
 */
void Application::sendAckMessage(const QJsonDocument& original_msg)
{
    std::cout << "Sending Ack back to FCM..." << std::endl;
    std::string to = original_msg.object().value("from").toString().toStdString();
    std::string mid = original_msg.object().value("message_id").toString().toStdString();

    Message msg(MessageType::ACK);
    msg.setTo(to);
    msg.setMessageId(mid);
    try
    {
        msg.validate();
        QJsonDocument ackmsg = msg.toJson();
        emit sendMessage(ackmsg);
    }
    catch(std::exception& ex)
    {
        PRINT_EXCEPTION_STRING(std::cout, ex);
    }
}


/*!
 * \brief Application::handleAckMessage
 *        FCM successfully recieved our 'downstream' message. FCM will try to deliver
 *        it to our 'target' until the 'ttl' hasn't expired.If for some reason, the message
 *        couldn't be deivered e.g the device was offline, upon reconnect it will recieve a
 *        '<TODO>' message from FCM. The device should then do a full sync with the BAL.
 * \param ack_msg
 */
void Application::handleAckMessage(int id, const QJsonDocument& ack_msg)
{
    std::cout << "-----------------------------------START Handle Ack Message-----------------------------------------" << std::endl;

    std::string mid = ack_msg.object().value(fcmfieldnames::MESSAGE_ID).toString().toStdString();
    std::cout << FCM_TAG(id) << "Recieved downstream 'ack' from FCM for message id:" << mid << std::endl;
    __downstreamMessages.erase(mid);

    // a slot just opened up, lets send another msg from the 'pending msg queue'
    if (__downstreamMessagesPending.empty() == false)
    {
        auto i = __downstreamMessagesPending.front();
        __downstreamMessagesPending.pop();

        std::string msgId = i->getMessageId();
        __downstreamMessages.emplace(msgId, i);
        emit sendMessage(*(i->getMsg()));
    }
    std::cout << "-----------------------------------END   Handle Ack Message-----------------------------------------" << std::endl;
}


/*!
 * \brief Application::handleNackMessage
 * \param nack_msg
 */
void Application::handleNackMessage(int id, const QJsonDocument& nack_msg)
{
    std::cout << "-----------------------------------START Handle Nack Message----------------------------------------" << std::endl;
    std::string msg_id = nack_msg.object().value(fcmfieldnames::MESSAGE_ID).toString().toStdString();
    std::string error = nack_msg.object().value(fcmfieldnames::ERROR).toString().toStdString();
    std::string error_desc = nack_msg.object().value(fcmfieldnames::ERROR_DESC).toString().toStdString();
    std::cout << FCM_TAG(id) << "Recieved 'nack' for message id:"<< msg_id
              << ", error:" << error
              << ", error description:" << error_desc << std::endl;

    if ( error == "SERVICE_UNAVAILABLE" ||
         error == "INTERNAL_SERVER_ERROR" ||
         error == "DEVICE_MESSAGE_RATE_EXCEEDED" ||
         error == "TOPICS_MESSAGE_RATE_EXCEEDED" ||
         error == "CONNECTION_DRAINING")
     {
           retryWithExponentialBackoff(msg_id);
     }
    std::cout << "-----------------------------------END   Handle Nack Message----------------------------------------" << std::endl;
}


/*!
 * \brief Application::handleBALSocketReadyRead
 */
void Application::handleBALSocketReadyRead()
{
    QTcpSocket* socket = (QTcpSocket*)sender();
    QDataStream in;
    in.setDevice(socket);

    QByteArray bytes;
    in.startTransaction();
    in >> bytes;
    if (!in.commitTransaction())
        return;

    std::cout << "===================================START NEW BAL MESSAGE============================================\n";
    std::cout << "Peer " << getPeerDetail(socket) << std::endl;
    try
    {
        QJsonDocument jsondoc = QJsonDocument::fromBinaryData(bytes);
        PRINT_JSON_DOC(std::cout, jsondoc);
        handleBALmsg(socket, jsondoc);
    }
    catch (std::exception& err)
    {
        PRINT_EXCEPTION_STRING(std::cout, err);
    }
    catch (...)
    {
      std::cout << "ERROR:Unknown exception caught." << std::endl;
    }
    std::cout << "===================================END NEW BAL MESSAGE==============================================\n";
}


/*!
 * \brief Application::handleBALmsg
 * \param cmd
 */
void Application::handleBALmsg(
            QTcpSocket* socket,
            const QJsonDocument& jsondoc)
{
    std::string type = jsondoc.object().value("message_type").toString().toStdString();
    if (type == "LOGON")
    {
         std::string sid = jsondoc.object().value("session_id").toString().toStdString();
         handleBALLogonRequest(socket, sid);
    }
    else if (type == "PASSTHRU")
    {
        //TODO maybe do some validation
        QJsonDocument downstream_msg;
        QJsonObject downstream_data = jsondoc.object().value(phantomfieldnames::FCM_DATA).toObject();
        downstream_msg.setObject(downstream_data);
        try
        {
            trySendingDownstreamMessage(downstream_msg);
        }
        catch (std::exception& err)
        {
            PRINT_EXCEPTION_STRING(std::cout, err);
        }
    }
    else if (type == "UPSTREAM_ACK")
    {
        std::string msgid = jsondoc.object().value(phantomfieldnames::MESSAGE_ID).toString().toStdString();
        handleBALAckMsg(msgid);
    }
    else
    {
        std::stringstream err;
        err << "Unknown message type<" << type << "> recieved from Peer["
            << getPeerDetail(socket) << "]";
        THROW_INVALID_ARGUMENT_EXCEPTION(err.str());
    }
}


/*!
 * \brief Application::handleNewBALConnection
 */
void Application::handleNewBALConnection()
{
    std::cout << "new connection..." << std::endl;
    QTcpSocket* socket = __serverSocket.nextPendingConnection();
    connect(socket, &QAbstractSocket::readyRead, this, &Application::handleBALSocketReadyRead);
    connect(socket, &QAbstractSocket::disconnected, this, &Application::handleBALSocketDisconnected);

    quintptr id = socket->socketDescriptor();
    BALConnPtr_t conn(new BALConn(socket));
    __sessionMapU.emplace(id, conn);

    auto timerCallback = [conn, this]{
            handleAuthenticationTimeout(conn);
      };
    QTimer::singleShot(MAX_LOGON_MSG_WAIT_TIME, Qt::TimerType::PreciseTimer, conn->getTimerContext(), timerCallback);
}


/*!
 * \brief Application::handleAuthenticationTimeout
 * \param sess
 */
void Application:: handleAuthenticationTimeout(BALConnPtr_t conn)
{
    std::cout << "-----------------------------------START Authenticatio----------------------------------------------" << std::endl;
    conn->getSocket()->blockSignals(true);
    __sessionMapU.erase(conn->getSocket()->socketDescriptor());
    std::cout << "Peer[" << getPeerDetail(conn->getSocket())
        << "] timed out before being authenticated. Destroying connection..." << std::endl;
    std::cout << "-----------------------------------END Authentication Timeout --------------------------------------" << std::endl;
}


/*!
 * \brief Application::getPeerDetail
 * \param socket
 * \return
 */
std::string Application::getPeerDetail(const QTcpSocket* socket)
{
    std::stringstream ss;
    ss << socket->peerName().toStdString() <<", IP Address:"
       << socket->peerAddress().toString().toStdString()
       << ", Port:" << socket->peerPort();
    return ss.str();
}


/*!
 * \brief Application::handleBALAckMsg
 * \param msgid
 */
void Application::handleBALAckMsg(const std::string& msgid)
{
    std::cout << "Recieved Ack for upstream message msgid[" << msgid << std::endl;
    __upstreamMessages.erase(msgid);
}


/*!
 * \brief Application::handleBALLogonRequest
 * \param socket
 * \param session_id
 */
void Application::handleBALLogonRequest(
                    QTcpSocket* socket,
                    const std::string& session_id)
{
    auto it = __sessionMapU.find(socket->socketDescriptor());
    if ( it == __sessionMapU.end())
    {
        std::cout << "ERROR:Logon request arrived but socket is now found in the system." << std::endl;
        return;
    }
    // At the end of this function, if 'conn' checks out
    // it will be accepted. Or else it will be discarded.
    // Let's also stop the timeout timer.
    BALConnPtr_t conn = it->second;
    conn->deleteTimerContext();
    __sessionMapU.erase(it);

    // check if its a known BAL client.
    auto it1 = __sessionMap.find(session_id);
    if ( it1 == __sessionMap.end())
    {
        std::cout << "ERROR: Unknown BAL session[" << session_id
                  << "]. Did u forget to setup the session in the 'config' file." << std::endl;
        return;
    }
    // all checks out. Accept 'conn'.
    BALSessionPtr_t sess = it1->second;
    sess->setConn(conn);
    sess->getConn()->getSocket()->setProperty("session_id", QVariant(session_id.c_str()));
    sess->setState(SessionState::AUTHENTICATED);

    QJsonDocument reply;
    QJsonObject root;
    root["message_type"] = "LOGON_RESPONSE";
    root["session_id"] = session_id.c_str();
    root["status"] = "SUCCESS";
    reply.setObject(root);
    sess->writeMessage(reply);
    std::cout << "===================================NEW BAL CLIENT CONNECTED=========================================" << std::endl;
    std::cout << "=        SESSION_ID:" <<  session_id << std::endl;
    std::cout << "=        PEER:"       << getPeerDetail(socket) << std::endl;
    std::cout << "====================================================================================================" << std::endl;

    resendPendingUpstreamMessages();
}


/*!
 * \brief Application::handleBALSocketDisconnected
 */
void Application::handleBALSocketDisconnected()
{
    std::cout << "-----------------------------------START BAL Session Disconnect-------------------------------------\n";
    QTcpSocket* socket = (QTcpSocket*) sender();
    std::cout << "Peer[" << getPeerDetail(socket) << "] disconnected." << std::endl;
    auto desc = socket->socketDescriptor();

    //case I.
    //disconnected before authentication.
    auto it = __sessionMapU.find(desc);
    if ( it != __sessionMapU.end())
    {
        it->second->deleteTimerContext();
        __sessionMapU.erase(it);
        std::cout << "Unauthenticated session lost." << std::endl;
    }else
    {
        // case II. disconnected after authentication.
        std::string sid = socket->property("session_id").toString().toStdString();
        auto it = __sessionMap.find(sid);
        if ( it != __sessionMap.end())
        {
            it->second->getConn().reset();
            it->second->setState(SessionState::UNAUTHENTICATED);
            std::cout << "Authenticated session <" << sid  <<">, lost." << std::endl;
        }
    }
    std::cout << "----------------------------------END BAL Session Disconnect----------------------------------------\n";
}


/*!
 * \brief Application::trySendingDownstreamMessage
 *  Flow control@ https://firebase.google.com/docs/cloud-messaging/server#flow
 *  "Every message sent to CCS receives either an ACK or a NACK response.
 *  Messages that haven't received one of these responses are considered pending.
 *  If the pending message count reaches 100,
 *  the app server should stop sending new messages and wait for
 *  CCS to acknowledge some of the existing pending messages"
 * \param downstream_msg
 */
void Application::trySendingDownstreamMessage(const QJsonDocument& downstream_msg)
{
    std::string mid = downstream_msg.object().value(fcmfieldnames::MESSAGE_ID).toString().toStdString();
    FcmMsgPtr_t msgptr(new QJsonDocument(downstream_msg));

    DownstreamMessagePtr_t dptr;
    dptr->setMessageId(mid);
    dptr->setMsg(msgptr);

    if (__downstreamMessages.size() < MAX_PENDING_MESSAGES)
    {
        __downstreamMessages.emplace(mid, dptr);
        emit sendMessage(downstream_msg);
    }else
    {
        std::cout << "WARNING: Max pending messages [" << MAX_PENDING_MESSAGES
                  << "] breached. Slowing down. Pls check system..." << std::endl;
        __downstreamMessagesPending.push(dptr);
    }
}


/*!
 * \brief Application::resendPendingUpstreamMessages
 */
void Application::resendPendingUpstreamMessages()
{
    for (auto& it: __upstreamMessages)
    {
        std::cout << "Resending mid[" << it.first << "]\n";
        emit sendMessage(*(it.second->getMsg()));
    }
}


/*!
 * \brief Application::hupSignalHandler
 */
void Application::hupSignalHandler(int)
{
    char a = 1;
    ::write(sighupFd[0], &a, sizeof(a));
}


/*!
 * \brief Application::termSignalHandler
 */
void Application::termSignalHandler(int)
{
    char a = 1;
    ::write(sigtermFd[0], &a, sizeof(a));
}


/*!
 * \brief Application::handleSigTerm
 */
void Application::handleSigTerm()
{
    snTerm->setEnabled(false);
    char tmp;
    ::read(sigtermFd[1], &tmp, sizeof(tmp));

    // do Qt stuff
    std::cout << "SIGTERM RECIEVED:" << std::endl;
    QCoreApplication::quit();

    snTerm->setEnabled(true);
}


/*!
 * \brief Application::handleSigHup
 */
void Application::handleSigHup()
{
  snHup->setEnabled(false);
  char tmp;
  ::read(sighupFd[1], &tmp, sizeof(tmp));

  // do Qt stuff
  std::cout << "SIGHUB RECIEVED:" << std::endl;

  snHup->setEnabled(true);
}

