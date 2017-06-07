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
    __balSessionMap.emplace(balclient.toStdString(), sess);
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
    connect(&fcmConn, SIGNAL(connectionStarted(int)),                   this, SLOT(handleFcmConnectionStarted(int)));
    connect(&fcmConn, SIGNAL(connectionEstablished(int)),               this, SLOT(handleFcmConnectionEstablished(int)));
    connect(&fcmConn, SIGNAL(connectionShutdownStarted(int)),           this, SLOT(handleFcmConnectionShutdownStarted(int)));
    connect(&fcmConn, SIGNAL(connectionShutdownCompleted(int)),         this, SLOT(handleFcmConnectionShutdownCompleted(int)));
    connect(&fcmConn, SIGNAL(connectionLost(int)),                      this, SLOT(handleFcmConnectionLost(int)));
    connect(&fcmConn, SIGNAL(connectionDrainingStarted(int)),           this, SLOT(handleFcmConnectionDrainingStarted(int)));
    connect(&fcmConn, SIGNAL(xmppHandshakeStarted(int)),                this, SLOT(handleFcmXmppHandshakeStarted(int)));
    connect(&fcmConn, SIGNAL(sessionEstablished(int)),                  this, SLOT(handleFcmSessionEstablished(int)));
    connect(&fcmConn, SIGNAL(streamClosed(int)),                        this, SLOT(handleFcmStreamClosed(int)));
    connect(&fcmConn, SIGNAL(heartbeatRecieved(int)),                   this, SLOT(handleFcmHeartbeatRecieved(int)));
    connect(&fcmConn, SIGNAL(connectionError(int, const QString&)),     this, SLOT(handleFcmConnectionError(int, const QString&)));
    connect(&fcmConn, SIGNAL(newMessage(int, const QJsonDocument&)),    this, SLOT(handleFcmNewUpstreamMessage(int, const QJsonDocument&)));
    connect(&fcmConn, SIGNAL(newAckMessage(int, const QJsonDocument&)), this, SLOT(handleFcmAckMessage(int, const QJsonDocument&)));
    connect(&fcmConn, SIGNAL(newNackMessage(int, const QJsonDocument&)),this, SLOT(handleFcmNackMessage(int, const QJsonDocument&)));


    //setup queued connection to fcm connection handle for sending downstream message to FCM.
    connect(this, SIGNAL(sendMessage(const QJsonDocument&)),&fcmConn, SLOT(handleSendMessage(const QJsonDocument&)));
}


/*!
 * \brief Application::handleStreamClosed
 */
void Application::handleFcmStreamClosed(int id)
{
    std::cout << FCM_TAG(id) <<"Recieved stream closed from FCM." << std::endl;
}


/*!
 * \brief Application::handleHeartbeatRecieved
 */
void Application::handleFcmHeartbeatRecieved(int id)
{
    std::cout <<  FCM_TAG(id) << "Recieved keepalive from FCM" << std::endl;
}


/*!
 * \brief Application::handleFcmConnectionError
 * \param id
 * \param err
 */
void Application::handleFcmConnectionError(int id, const QString& err)
{
    std::cout << FCM_TAG(id) << "ERROR. Error string[" << err.toStdString() << "]" << std::endl;
}


/*!
 * \brief Application::handleConnectionStarted
 */
void Application::handleFcmConnectionStarted(int id)
{
    std::cout << FCM_TAG(id) << "Connecting to FCM server..." << std::endl;
}


/*!
 * \brief Application::handleConnectionEstablished
 */
void Application::handleFcmConnectionEstablished(int id)
{
    std::cout << FCM_TAG(id) << "Secure TLS channel established with FCM server..." << std::endl;
}


/*!
 * \brief Application::handleXmppHandshakeStarted
 */
void Application::handleFcmXmppHandshakeStarted(int id)
{
    std::cout << FCM_TAG(id) <<"Starting XMPP handshake. Opening stream..." << std::endl;
}


/*!
 * \brief Application::handleSessionEstablished
 */
void Application::handleFcmSessionEstablished(int id)
{
    std::cout << FCM_TAG(id) << "Session authenticated sucessfully.." << std::endl;
    std::cout << "----------------------------------------------------------------------------------------------------\n";
    std::cout << "-     SESSION WITH FCM ESTABLISHED SUCCESSFULLY        -" << std::endl;
    std::cout << "-     SESSION ID: " << __fcmServerId.toStdString() <<"\n";
    std::cout << "----------------------------------------------------------------------------------------------------" << std::endl;
}


/*!
 * \brief Application::handleFcmConnectionLost
 */
void Application::handleFcmConnectionLost(int id)
{
    std::cout << FCM_TAG(id) << "Disconnected to FCM server." << std::endl;
}


/*!
 * \brief Application::handleConnectionShutdownStarted
 */
void Application::handleFcmConnectionShutdownStarted(int id)
{
    std::cout << FCM_TAG(id) <<"Shuting down connection to FCM..." << std::endl;
}


/*!
 * \brief Application::handleConnectionShutdownCompleted
 */
void Application::handleFcmConnectionShutdownCompleted(int id)
{
    std::cout << FCM_TAG(id) << "Connection to FCM shutdown successfully." << std::endl;
}


/*!
 * \brief Application::handleConnectionDrainingStarted
 * \param id
 */
void Application::handleFcmConnectionDrainingStarted(int id)
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
 * \brief Application::handleNewUpstreamMessage - we convert 'client_msg' to an internal GIMMM
 *  message format and store it in a temporary storage before forwarding it to the BAL session
 *  it was intended for. We then wait for an 'upstream_ack' message from the BAL.
 *
 * \param client_msg - The json message sent by the app.
 */
void Application::handleFcmNewUpstreamMessage(
                                        int id,
                                        const QJsonDocument& client_msg)
{

    std::cout << "-----------------------------------START Handle New Upstream Message-----------------------------------------" << std::endl;
    std::cout << FCM_TAG(id) << "[" << client_msg.toJson().toStdString() << "]" << std::endl;
    try
    {
        std::string from = client_msg.object().value(fcmfieldnames::FROM).toString().toStdString();
        std::string sessionid = client_msg.object().value(fcmfieldnames::CATEGORY).toString().toStdString();
        std::string mid = client_msg.object().value(fcmfieldnames::MESSAGE_ID).toString().toStdString();

        std::cout << "Recieved 'upstream' message with msg id [" << mid<< "] from:" << from << std::endl;
        std::cout << "Target session id:" << sessionid  << std::endl;


        // add gimmm header before sending to BAL.
        QJsonDocument gimmm_msg;
        QJsonObject root;
        root[gimmmfieldnames::MESSAGE_TYPE] = "UPSTREAM",
        root[gimmmfieldnames::SESSION_ID]   =  sessionid.c_str();
        root[gimmmfieldnames::FCM_DATA] = client_msg.object();
        gimmm_msg.setObject(root);

        PayloadPtr_t pmsg(new QJsonDocument(gimmm_msg));
        MessagePtr_t msgptr(new Message());
        msgptr->setType(MessageType::UPSTREAM);
        msgptr->setMessageId(mid);
        msgptr->setPayload(pmsg);
        msgptr->setTargetSessionId(sessionid);
        msgptr->setSourceSessionId("fcm");

        __dbConn.saveMsg(*msgptr);
        // Save successfull, send ack back to FCM.
        sendAckMessage(client_msg);

        std::cout << "Start forwarding message to BAL...." << std::endl;
        // forward the message to the intended BAL.
        MessageManager& msgmanager = findBalMessageManager(sessionid);
        msgmanager.add(msgptr);
        int rcode = msgmanager.canSend(msgptr);
        switch (rcode)
        {
            case 0:
            {
                forwardMsg(msgptr);
                break;
            }
            case 1:
            {
                std::cout << "Failed to forward message. Message is not in the NEW state."
                          << std::endl;
                break;
            }
            case 2:
            {
                std::cout << "Failed to forward message. Max pending messages[" << MAX_PENDING_MESSAGES
                          << "] breached" << std::endl;
                break;
            }
            default:
            {
                std::cout << "Failed to foward upstream message with rcode[" << rcode << "]"
                          << std::endl;
            }
        }
    }
    catch (std::exception& err)
    {
        PRINT_EXCEPTION_STRING(std::cout, err);
    }
    catch (...)
    {

    }
    std::cout << "-----------------------------------END Handle New Upstream Message-----------------------------------------" << std::endl;
}


/*!
 * \brief Application::handleFcmAckMessage
 *        FCM successfully recieved our 'downstream' message. FCM will try to deliver
 *        it to our 'target' until the 'ttl' hasn't expired.If for some reason, the message
 *        couldn't be deivered e.g the device was offline, upon reconnect it will recieve a
 *        '<TODO>' message from FCM. The device should then do a full sync with the BAL.
 * \param ack_msg
 */
void Application::handleFcmAckMessage(int id, const QJsonDocument& ack_msg)
{
    std::cout << "-----------------------------------START Handle Ack Message-----------------------------------------" << std::endl;

    std::string mid = ack_msg.object().value(fcmfieldnames::MESSAGE_ID).toString().toStdString();
    std::cout << FCM_TAG(id) << "Recieved downstream 'ack' from FCM for message id:" << mid << std::endl;

    try
    {
        MessagePtr_t msg = __fcmMsgManager.findMessage(mid);

        Message temp_msg = *msg;
        temp_msg.setState(MessageState::DELIVERED);
        __dbConn.updateMsg(temp_msg);
        *msg = temp_msg;
        __fcmMsgManager.remove(mid);
        // a slot just opened up, lets send another msg from the 'pending msg queue' to fcm
        sendNextPendingDownstreamMessage(__fcmMsgManager);


        //fwd to BAL
        forwardAckMsg(mid);
    }
    catch (std::exception& err)
    {
        PRINT_EXCEPTION_STRING(std::cout, err);
    }
    catch (...)
    {

    }

    std::cout << "-----------------------------------END   Handle Ack Message-----------------------------------------" << std::endl;
}


/*!
 * \brief Application::handleNackMessage
 * \param nack_msg
 */
void Application::handleFcmNackMessage(int id, const QJsonDocument& nack_msg)
{
    std::cout << "-----------------------------------START Handle Nack Message----------------------------------------" << std::endl;
    std::string msg_id = nack_msg.object().value(fcmfieldnames::MESSAGE_ID).toString().toStdString();
    std::string error = nack_msg.object().value(fcmfieldnames::ERROR).toString().toStdString();
    std::string error_desc = nack_msg.object().value(fcmfieldnames::ERROR_DESC).toString().toStdString();
    std::cout << FCM_TAG(id) << "Recieved 'nack' for message id:"<< msg_id
              << ", error:" << error
              << ", error description:" << error_desc << std::endl;

    try
    {
        MessagePtr_t origmsg = __fcmMsgManager.findMessage(msg_id);
        if ( error == "SERVICE_UNAVAILABLE" ||
             error == "INTERNAL_SERVER_ERROR" ||
             error == "DEVICE_MESSAGE_RATE_EXCEEDED" ||
             error == "TOPICS_MESSAGE_RATE_EXCEEDED" ||
             error == "CONNECTION_DRAINING")
         {

            Message temp_msg = *origmsg;
            temp_msg.setState(MessageState::NACK);
            __dbConn.saveMsg(temp_msg);
            *origmsg = temp_msg;

            retryNacksWithExponentialBackoff(origmsg);
         }else
         {
            __fcmMsgManager.remove(msg_id);
            origmsg->setState(MessageState::DELIVERY_FAILED);
            __dbConn.updateMsg(*origmsg);
            // new slot opened.send another pending message.
            sendNextPendingDownstreamMessage(__fcmMsgManager);

            // notify bal of delivery failure
            notifyDownstreamUploadFailure(origmsg);
         }
    }
    catch(std::exception& err)
    {
        PRINT_EXCEPTION_STRING(std::cout, err);
    }
    catch (...)
    {

    }

    std::cout << "-----------------------------------END   Handle Nack Message----------------------------------------" << std::endl;
}


/*!
 * \brief Application::sendNextPendingDownstreamMessage
 * \param msgmanager
 */
void Application::sendNextPendingDownstreamMessage(const MessageManager& msgmanager)
{
    MessagePtr_t nextmsg = msgmanager.getNext();
    if ( nextmsg)
    {
        std::cout << "Sending downstream message with msgid ["
                  << nextmsg->getMessageId() << "] from pending queue." << std::endl;
        uploadToFcm(nextmsg);
        // print warning as necessary.
        if (msgmanager.isMessagePending())
        {
            std::int64_t i = msgmanager.getPendingAckCount();
            std::cout << "WARNING: FCM too slow to ack.[" << i
                      << "] messages are still pending to be send to FCM." << std::endl;
        }
    }
}


/*!
 * \brief Application::sendNextPendingUpstreamMessage
 * \param msgmanager
 */
void Application::sendNextPendingUpstreamMessage(const MessageManager& msgmanager)
{
    MessagePtr_t nextmsg = msgmanager.getNext();
    if ( nextmsg)
    {
        std::cout << "Sending upstream message with msgid ["
                  << nextmsg->getMessageId() << "] from pending queue." << std::endl;
        const QJsonDocument& jdoc = *(nextmsg->getPayload());

        SessionId_t sessid = nextmsg->getTargetSessionId();
        BALSessionPtr_t sess = findBalSession(sessid);

        sess->writeMessage(jdoc);

        // print warning as necessary.
        if (msgmanager.isMessagePending())
        {
            std::int64_t i = msgmanager.getPendingAckCount();
            std::cout << "WARNING: BAL is too slow to ack.[" << i
                      << "] messages are still pending to be send to FCM." << std::endl;
        }
    }
}


/*!
 * \brief Application::forwardAckMsg
 * \param original_msgid
 */
void Application::forwardAckMsg(const MessageId_t& original_msgid)
{

    try
    {
        MessagePtr_t origmsg = __fcmMsgManager.findMessage(original_msgid);
        // ack goes to the source session.
        SessionId_t sessid = origmsg->getSourceSessionId();

        // add gimmm header and foward it to BAL.
        QJsonDocument gimmm_msg;
        QJsonObject root;
        root[gimmmfieldnames::MESSAGE_TYPE] = "DOWNSTREAM_ACK",
        root[gimmmfieldnames::SESSION_ID]   =  sessid.c_str();
        root[gimmmfieldnames::MESSAGE_ID]   =  original_msgid.c_str();
        gimmm_msg.setObject(root);

        PayloadPtr_t pmsg(new QJsonDocument(gimmm_msg));
        SequenceId_t newseqid = __dbConn.getNextSequenceId();

        std::stringstream newmsgid;
        newmsgid << newseqid;
        MessagePtr_t balack = Message::createMessage(
                                  newseqid,
                                  MessageType::DOWNSTREAM_ACK,
                                  newmsgid.str(),
                                  "",
                                  sessid,
                                  pmsg);

        __dbConn.saveMsg(*balack);

        MessageManager& msgmanager = findBalMessageManager(sessid);
        msgmanager.add(balack);
        if (msgmanager.canSend(balack))
        {
            forwardMsg(balack);
        }
     }
    catch (std::exception& err)
    {
        PRINT_EXCEPTION_STRING(std::cout, err);
    }
    catch (...)
    {

    }
}


MessageManager& Application::findBalMessageManager(const SessionId_t& session_id)
{
    auto it = __balSessionMap.find(session_id);
    if ( it != __balSessionMap.end())
    {
        BALSessionPtr_t sess = it->second;
        return sess->getMessageManager();
    }
    std::stringstream err;
    err << "Cannot find message manager for session id["
        << session_id << "]" << std::endl;
    THROW_INVALID_ARGUMENT_EXCEPTION(err.str());
}


/*!
 * \brief Application::forwardMsg
 * \param msg
 */
void Application::forwardMsg( MessagePtr_t& msg)
{
    SessionId_t session_id = msg->getTargetSessionId();
    std::cout << "Forwarding message to sessionid [" << session_id
              << "]" << std::endl;
    BALSessionPtr_t sess = findBalSession(session_id);

    if (msg->getState() != MessageState::PENDING_ACK)
    {
        Message temp_msg = *msg;
        temp_msg.setState(MessageState::PENDING_ACK);
        __dbConn.updateMsg(temp_msg);
        *msg = temp_msg;
        sess->getMessageManager().incrementPendingAckCount();
    }

    auto gimmm_msg = msg->getPayload();
    const QJsonDocument& jdoc = *gimmm_msg;
    sess->writeMessage(jdoc);
}


/*!
 * \brief Application::retrySendingDownstreamMessage
 * \param ptr
 */
void Application::retrySendingDownstreamMessage(MessagePtr_t msg)
{
    uploadToFcm(msg);
}


/*!
 * \brief Application::notifyDownstreamUploadFailure
 * \param ptr
 */
void Application::notifyDownstreamUploadFailure(const MessagePtr_t& msg)
{
    const QJsonDocument& jdoc = *(msg->getPayload());
    std::cout << "ERROR: Droping downstream message\n.["
              << jdoc.toJson().toStdString()
              << "], Max retry [" << MAX_DOWNSTREAM_UPLOAD_RETRY <<"] reached." << std::endl;

    try
    {
        const MessageId_t& msgid = msg->getMessageId();
        //failure goes to the source session.
        const SessionId_t& sessid = msg->getSourceSessionId();

        QJsonDocument gimmm_msg;
        QJsonObject root;
        root[gimmmfieldnames::MESSAGE_TYPE] = "DOWNSTREAM_REJECT";
        root[gimmmfieldnames::MESSAGE_ID] = msgid.c_str();
        root[gimmmfieldnames::SESSION_ID] = sessid.c_str();
        root[gimmmfieldnames::ERROR_STRING]= "Max retry reached.";
        gimmm_msg.setObject(root);

        PayloadPtr_t pmsg(new QJsonDocument(gimmm_msg));
        MessagePtr_t msgptr(new Message());
        msgptr->setType(MessageType::DOWNSTREAM_REJECT);
        msgptr->setMessageId(msgid);
        msgptr->setPayload(pmsg);
        msgptr->setTargetSessionId(sessid);
        msgptr->setState(MessageState::PENDING_ACK);
        __dbConn.saveMsg(*msgptr);

        auto sess = findBalSession(sessid);
        sess->writeMessage(jdoc);
    }
    catch( std::exception& err)
    {
        PRINT_EXCEPTION_STRING(std::cout, err);
    }
    catch (...)
    {

    }
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

    QJsonDocument ackmsg;
    QJsonObject root;
    root[fcmfieldnames::TO] = to.c_str();
    root[fcmfieldnames::MESSAGE_ID] = mid.c_str();
    root[fcmfieldnames::MESSAGE_TYPE] = "ack";
    ackmsg.setObject(root);

    PRINT_JSON_DOC(std::cout, ackmsg);
    // ack as many and as quickly as possible.
    emit sendMessage(ackmsg);
}


/*!
 * \brief Application::retryNacksWithExponentialBackoff
 * \param msg_id
 */
void Application::retryNacksWithExponentialBackoff(MessagePtr_t ptr)
{
    bool success =  retryWithBackoff(ptr);

    if (!success)
    {
        // retry exceeded send failure.
        notifyDownstreamUploadFailure(ptr);
    }
}


bool Application::retryWithBackoff(MessagePtr_t& msg)
{
    int msec = msg->getNextRetryTimeout();
    if ( msec != -1)
    {
        auto timerCallback = [msg, this]{
            retrySendingDownstreamMessage(msg);
        };
        // retry sending after 'msec' millisecs.
        QTimer::singleShot(msec, Qt::TimerType::PreciseTimer, timerCallback);
        return true;
    }
    std::cout << "ERROR: Unable to send msg with msgid[" << msg->getMessageId()
              << "]. Max retry reached." << std::endl;
    return false;
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
    std::string type = jsondoc.object().value(gimmmfieldnames::MESSAGE_TYPE).toString().toStdString();
    std::string session_id  = jsondoc.object().value(gimmmfieldnames::SESSION_ID).toString().toStdString();
    if (type == "BAL_LOGON")
    {
         handleBALLogonRequest(socket, session_id);
    }
    else if (type == "BAL_PASSTHRU")
    {
        handleBalPassthruMessage(session_id, jsondoc);
    }
    else if (type == "BAL_ACK")
    {
        std::string msgid = jsondoc.object().value(gimmmfieldnames::MESSAGE_ID).toString().toStdString();
        handleBalAckMsg(session_id, msgid);
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
    __balSessionMapU.emplace(id, conn);

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
    __balSessionMapU.erase(conn->getSocket()->socketDescriptor());
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
 * \brief Application::handleBalAckMsg
 * \param msgid
 */
void Application::handleBalAckMsg(
        const SessionId_t& session_id,
        const MessageId_t& msg_id)
{
    std::cout << "Recieved Ack for upstream message msgid[" << msg_id
              << "], from sessionid [" <<session_id << "]" << std::endl;
    try
    {
        MessageManager& msgmanager = findBalMessageManager(session_id);
        MessagePtr_t msg = msgmanager.findMessage(msg_id);
        Message temp_msg = *msg;
        temp_msg.setState(MessageState::DELIVERED);
        __dbConn.updateMsg(temp_msg);
        *msg = temp_msg;

        msgmanager.remove(msg_id);
        sendNextPendingUpstreamMessage(msgmanager);
    }
    catch ( std::exception& err)
    {
        PRINT_EXCEPTION_STRING(std::cout, err);
    }
    catch (...)
    {

    }
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
    auto it = __balSessionMapU.find(socket->socketDescriptor());
    if ( it == __balSessionMapU.end())
    {
        std::cout << "ERROR:Logon request arrived but socket is now found in the system." << std::endl;
        return;
    }
    // At the end of this function, if 'conn' checks out
    // it will be accepted. Or else it will be discarded.
    // Let's also stop the timeout timer.
    BALConnPtr_t conn = it->second;
    conn->deleteTimerContext();
    __balSessionMapU.erase(it);

    // check if its a known BAL client.
    auto it1 = __balSessionMap.find(session_id);
    if ( it1 == __balSessionMap.end())
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

    resendPendingUpstreamMessages(sess);
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
    auto it = __balSessionMapU.find(desc);
    if ( it != __balSessionMapU.end())
    {
        it->second->deleteTimerContext();
        __balSessionMapU.erase(it);
        std::cout << "Unauthenticated session lost." << std::endl;
    }else
    {
        // case II. disconnected after authentication.
        std::string sid = socket->property("session_id").toString().toStdString();
        auto it = __balSessionMap.find(sid);
        if ( it != __balSessionMap.end())
        {
            it->second->getConn().reset();
            it->second->setState(SessionState::UNAUTHENTICATED);
            std::cout << "Authenticated session <" << sid  <<">, lost." << std::endl;
        }
    }
    std::cout << "----------------------------------END BAL Session Disconnect----------------------------------------\n";
}


/*!
 * \brief Application::handleBalPassthruMessage
 *  Flow control@ https://firebase.google.com/docs/cloud-messaging/server#flow
 *  "Every message sent to CCS receives either an ACK or a NACK response.
 *  Messages that haven't received one of these responses are considered pending.
 *  If the pending message count reaches 100,
 *  the app server should stop sending new messages and wait for
 *  CCS to acknowledge some of the existing pending messages"
 * \param session_id    session from where this message was recieved.
 * \param downstream_msg
 */
void Application::handleBalPassthruMessage(
    const SessionId_t& session_id,
    const QJsonDocument& downstream_msg)
{
    std::string mid = downstream_msg.object().value(gimmmfieldnames::MESSAGE_ID).toString().toStdString();
    std::string gid = downstream_msg.object().value(gimmmfieldnames::GROUP_ID).toString().toStdString();
    QJsonObject data = downstream_msg.object().value(gimmmfieldnames::FCM_DATA).toObject();

    QJsonDocument jdoc(data);
    PayloadPtr_t pmsg(new QJsonDocument(jdoc));

    SequenceId_t nextseqid = __dbConn.getNextSequenceId();
    MessagePtr_t msg = Message::createMessage( nextseqid,
                                                MessageType::DOWNSTREAM,
                                                mid,
                                                gid,
                                                session_id,
                                                pmsg
                                                     );
    __dbConn.saveMsg(*msg);
    __fcmMsgManager.add(msg);
    if (__fcmMsgManager.canSend(msg))
    {
        uploadToFcm(msg);
    }
}

/*!
 * \brief Application::uploadToFcm
 * \param msg
 */
void Application::uploadToFcm(MessagePtr_t &msg)
{
    Message temp_msg = *msg;
    temp_msg.setState(MessageState::PENDING_ACK);
    __dbConn.updateMsg(temp_msg);
    *msg = temp_msg;
    __fcmMsgManager.incrementPendingAckCount();

    const QJsonDocument& jdoc = *(msg->getPayload());
    emit sendMessage(jdoc);
}

/*!
 * \brief Application::resendPendingUpstreamMessages
 */
void Application::resendPendingUpstreamMessages(const BALSessionPtr_t& sess)
{
    MessageManager& msgmanager  = sess->getMessageManager();
    const SessionId_t& sid      = sess->getSessionId();

    MessageQueue_t& msgmap = msgmanager.getMessages();
    for (auto& it: msgmap)
    {
        MessagePtr_t msg = it.second;
        // resend new and pending ack messages.
        int rcode = msgmanager.canSendOnReconnect(msg);
        if (rcode == 0)
        {
            std::cout << "Resending message with mid[" << msg->getMessageId()
                      << "] to session [" << sid <<"]" << std::endl;

            forwardMsg(msg);
        }
        else if ( rcode == 2 || rcode == 3)
        {
            bool success = retryWithBackoff(msg);
            if (!success)
               msgmanager.remove(msg->getMessageId());
        }
        else
        {
           std::cout << "ERROR: Unable to send message with id[" << msg->getMessageId()
                     << "]" << std::endl;
           msgmanager.remove(msg->getMessageId());
        }
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


BALSessionPtr_t Application::findBalSession(const std::string& session_id)
{
    auto it1 = __balSessionMap.find(session_id);
    if ( it1 == __balSessionMap.end())
    {
        std::stringstream err;
        err << "ERROR: Unknown BAL session[" << session_id
                  << "]";
        THROW_INVALID_ARGUMENT_EXCEPTION(err.str());
    }
    BALSessionPtr_t sess = it1->second;
    return sess;
}
