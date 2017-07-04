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
#include <QFileInfo>

#include <iostream>
#include <cstdio>
#include <sstream>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>


int Application::sigintFd[2];
int Application::sigtermFd[2];


/*!
 * \brief Application::Application
 */
Application::Application()
    :__fcmConnCount(0),
     __fcmMsgManager(std::string("fcm"))
{
    start();
}


/*!
 * \brief Application::~Application
 */
Application::~Application()
{
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

    // load pending messages
    std::cout << "Loading downstream pending messages..." << std::endl;
    __dbConn.loadPendingMessages(__fcmMsgManager);
    std::cout << "Loaded[" << __fcmMsgManager.getMessages().size()
              <<  "] pending downstream messages.\n" << std::endl;
    for ( auto &&i : __balSessionMap)
    {
        MessageManager& msgmanager = i.second->getMessageManager();
        std::cout << "Loading upstream pending messages for bal session["
                  << msgmanager.getSessionId() << "]..."<< std::endl;
        __dbConn.loadPendingMessages(msgmanager);
        std::cout << "Loaded[" << msgmanager.getMessages().size()
              <<  "] pending downstream messages.\n" << std::endl;
    }

    //connect to fcm.
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

    QString filename;
    
    if (QFileInfo::exists("./config.ini"))
    {
        filename = "./config.ini";
    }else
    {
        std::cout << "ERROR. Configuration file 'config.ini' missing. Please do the following:\n"
                  << "1) Copy 'config.copy.ini' as 'config.ini' into the same dir as the executable.\n"
                  << "2) Update 'config.ini' with the appropriate credential and try again. Exiting..."
                  << std::endl;
        exit(0);
    }

    QSettings ini(filename, QSettings::IniFormat);

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

    // TODO support more than one.
    // BAL session SECTION
    QString balclient = ini.value("BAL_SECTION/session_id", "NULL").toString();
    if ( balclient == "NULL")
    {
        std::cout<< "ERROR: No BAL client session found. Exiting..." << std::endl;
        exit(0);
    }
    BALSessionPtr_t sess(new BALSession(balclient.toStdString()));
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
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sigintFd))
        qFatal("Couldn't create HUP socketpair");

    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sigtermFd))
        qFatal("Couldn't create TERM socketpair");

    snHup = new QSocketNotifier(sigintFd[1], QSocketNotifier::Read, this);
    connect(snHup, SIGNAL(activated(int)), this, SLOT(handleSigInt()));
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
    connect(&fcmConn, SIGNAL(newReceiptMessage(int, const QJsonDocument&)),this, SLOT(handleFcmReceiptMessage(int, const QJsonDocument&)));


    //setup queued connection to fcm connection handle for sending downstream message to FCM.
    connect(this, SIGNAL(sendMessage(const QJsonDocument&)),&fcmConn, SLOT(handleSendMessage(const QJsonDocument&)));
}


/*!
 * \brief Application::handleStreamClosed
 */
void Application::handleFcmStreamClosed(int id)
{
    std::cout << FCM_TAG_RX(id) <<"Recieved stream closed from FCM." << std::endl;
}


/*!
 * \brief Application::handleHeartbeatRecieved
 */
void Application::handleFcmHeartbeatRecieved(int id)
{
    std::cout <<  FCM_TAG_RX(id) << "Recieved keepalive from FCM" << std::endl;
}


/*!
 * \brief Application::handleFcmConnectionError
 * \param id
 * \param err
 */
void Application::handleFcmConnectionError(int id, const QString& err)
{
    std::cout << FCM_TAG_RX(id) << "ERROR. Error string[" << err.toStdString() << "]" << std::endl;
}


/*!
 * \brief Application::handleConnectionStarted
 */
void Application::handleFcmConnectionStarted(int id)
{
    std::cout << FCM_TAG_TX(id) << "Connecting to FCM server..." << std::endl;
}


/*!
 * \brief Application::handleConnectionEstablished
 */
void Application::handleFcmConnectionEstablished(int id)
{
    std::cout << FCM_TAG_RX(id) << "Secure TLS channel established with FCM server..." << std::endl;
}


/*!
 * \brief Application::handleXmppHandshakeStarted
 */
void Application::handleFcmXmppHandshakeStarted(int id)
{
    std::cout << FCM_TAG_TX(id) <<"Starting XMPP handshake. Opening stream..." << std::endl;
}


/*!
 * \brief Application::handleSessionEstablished
 */
void Application::handleFcmSessionEstablished(int id)
{
    std::cout << FCM_TAG_RX(id) << "Session authenticated sucessfully.." << std::endl;
    std::cout << "----------------------------------------------------------------------------------------------------\n";
    std::cout << "-     SESSION WITH FCM ESTABLISHED SUCCESSFULLY        -" << std::endl;
    std::cout << "-     SESSION ID: " << __fcmServerId.toStdString() <<"\n";
    std::cout << "----------------------------------------------------------------------------------------------------" << std::endl;

    resendAllPendingDownstreamMessages();
}

/*!
 * \brief Application::handleFcmConnectionLost
 */
void Application::handleFcmConnectionLost(int id)
{
    std::cout << FCM_TAG_RX(id) << "Disconnected to FCM server.\n" << std::endl;
}


/*!
 * \brief Application::handleConnectionShutdownStarted
 */
void Application::handleFcmConnectionShutdownStarted(int id)
{
    std::cout << FCM_TAG_RX(id) <<"Shuting down connection to FCM..." << std::endl;
}


/*!
 * \brief Application::handleConnectionShutdownCompleted
 */
void Application::handleFcmConnectionShutdownCompleted(int id)
{
    std::cout << FCM_TAG_RX(id) << "Connection to FCM shutdown successfully." << std::endl;
}


/*!
 * \brief Application::handleConnectionDrainingStarted
 * \param id
 */
void Application::handleFcmConnectionDrainingStarted(int id)
{
    std::cout << FCM_TAG_RX(id) << "Connection draining started..." << std::endl;

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
 * \brief Application::handleFcmNewUpstreamMessage - we convert 'client_msg' to an internal GIMMM
 *  message format and store it in a temporary storage before forwarding it to the BAL session
 *  it was intended for. We then wait for an 'upstream_ack' message from the BAL.
 *
 * \param client_msg - The json message sent by the app.
 */
void Application::handleFcmNewUpstreamMessage(
                                        int id,
                                        const QJsonDocument& client_msg)
{
    std::cout << FCM_TAG_RX(id);
    PRINT_JSON_DOC_RAW(std::cout, client_msg);
    std::cout << std::endl;

    std::cout << "-----------------------------------Start handleFcmNewUpstreamMessage-----------------------------------------" << std::endl;
    try
    {
        std::string from = client_msg.object().value(fcmfieldnames::FROM).toString().toStdString();
        std::string sessionid = client_msg.object().value(fcmfieldnames::CATEGORY).toString().toStdString();
        std::string fcm_mid = client_msg.object().value(fcmfieldnames::MESSAGE_ID).toString().toStdString();

        std::cout << "Recieved 'upstream' message with msg id [" << fcm_mid<< "] from:" << from << std::endl;
        std::cout << "Target session id: " << sessionid  << std::endl;

        SequenceId_t nextseqid = __dbConn.getNextSequenceId();

        // create gimmm message.
        QJsonDocument gimmm_msg;
        QJsonObject root;
        root[gimmmfieldnames::SEQUENCE_ID] = nextseqid;
        root[gimmmfieldnames::MESSAGE_TYPE] = "UPSTREAM",
        root[gimmmfieldnames::SESSION_ID]   =  sessionid.c_str();
        root[gimmmfieldnames::FCM_DATA] = client_msg.object();
        gimmm_msg.setObject(root);
        PayloadPtr_t pmsg(new QJsonDocument(gimmm_msg));

        Message* msgp = new Message( nextseqid,
                                     MessageType::UPSTREAM,
                                     fcm_mid,
                                     "",
                                     "fcm",
                                     sessionid,
                                     pmsg);

        MessagePtr_t msgptr(msgp);

        std::cout << "New Message created:" << std::endl;
        std::cout << *msgptr << std::endl;

        __dbConn.saveMsg(*msgptr);
        // Save successfull, send ack back to FCM.
        sendFcmAckMessage(client_msg);
        // Lets forward msg to the bal message.
        forwardMsgToBalsession(sessionid, msgptr);
    }
    catch (std::exception& err)
    {
        PRINT_EXCEPTION_STRING(std::cout, err);
    }
    catch (...)
    {
        std::cout << "ERROR: Unknown message caught." << std::endl;
    }
    std::cout << "-----------------------------------End handleFcmNewUpstreamMessage-----------------------------------------" << std::endl;
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
    std::cout << "-----------------------------------Start handleFcmAckMessage -----------------------------------------" << std::endl;
    PRINT_JSON_DOC_RAW(std::cout, ack_msg);

    std::string mid = ack_msg.object().value(fcmfieldnames::MESSAGE_ID).toString().toStdString();
    std::cout << FCM_TAG_RX(id) << "Received downstream 'ack' from FCM for message id:" << mid << std::endl;

    try
    {
        MessagePtr_t msg = __fcmMsgManager.findMessageWithFcmMsgId(mid);
        SessionId_t sessid = msg->getSourceSessionId();

        __dbConn.updateMsgState(*msg, MessageState::DELIVERED);
        __fcmMsgManager.removeMessageWithFcmMsgId(mid);

        // a slot just opened up, lets send another msg from the 'pending msg queue' to fcm
        sendNextPendingDownstreamMessage(__fcmMsgManager);

        //fwd to BAL
        std::cout << "Forwarding downstream Ack msg to sessionid:" << sessid << std::endl;
        SequenceId_t newseqid = __dbConn.getNextSequenceId();

        // add gimmm header and foward it to BAL.
        QJsonDocument gimmm_msg;
        QJsonObject root;
        root[gimmmfieldnames::SEQUENCE_ID]  = newseqid;
        root[gimmmfieldnames::MESSAGE_TYPE] = "DOWNSTREAM_ACK",
        root[gimmmfieldnames::SESSION_ID]   = sessid.c_str();
        root[gimmmfieldnames::FCM_DATA]     = ack_msg.object();
        gimmm_msg.setObject(root);

        PayloadPtr_t pmsg(new QJsonDocument(gimmm_msg));

        std::string fcm_mid = ack_msg.object().value(fcmfieldnames::MESSAGE_ID).toString().toStdString();
        MessagePtr_t balack( new Message(
                                  newseqid,
                                  MessageType::DOWNSTREAM_ACK,
                                  fcm_mid,
                                  "",
                                  "fcm",
                                  sessid,
                                  pmsg));

        __dbConn.saveMsg(*balack);
        forwardMsgToBalsession(sessid, balack);
    }
    catch (std::exception& err)
    {
        PRINT_EXCEPTION_STRING(std::cout, err);
    }
    catch (...)
    {
        std::cout << "ERROR: Unknown message caught." << std::endl;
    }
    std::cout << "-----------------------------------End handleFcmAckMessage -----------------------------------------" << std::endl;
}


/*!
 * \brief Application::handleNackMessage
 * \param nack_msg
 */
void Application::handleFcmNackMessage(int id, const QJsonDocument& nack_msg)
{
    std::cout << "-----------------------------------Start Handle Nack Message----------------------------------------" << std::endl;
    PRINT_JSON_DOC_RAW(std::cout, nack_msg);

    std::string msg_id = nack_msg.object().value(fcmfieldnames::MESSAGE_ID).toString().toStdString();
    std::string error  = nack_msg.object().value(fcmfieldnames::ERROR).toString().toStdString();
    std::string error_desc = nack_msg.object().value(fcmfieldnames::ERROR_DESC).toString().toStdString();
    std::cout << FCM_TAG_RX(id) << "Received 'nack' for message id:"<< msg_id
              << ", error:" << error
              << ", error description:" << error_desc << std::endl;

    try
    {
        MessagePtr_t origmsg = __fcmMsgManager.findMessageWithFcmMsgId(msg_id);
        if ( error == "SERVICE_UNAVAILABLE" ||
             error == "INTERNAL_SERVER_ERROR" ||
             error == "DEVICE_MESSAGE_RATE_EXCEEDED" ||
             error == "TOPICS_MESSAGE_RATE_EXCEEDED" ||
             error == "CONNECTION_DRAINING")
         {
            retryDownstreamWithExponentialBackoff(origmsg);
         }else
         {
            __dbConn.updateMsgState(*origmsg, MessageState::DELIVERY_FAILED);
            __fcmMsgManager.removeMessageWithFcmMsgId(msg_id);
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
        std::cout << "ERROR: Unknown message caught." << std::endl;
    }

    std::cout << "-----------------------------------End Handle Nack Message----------------------------------------" << std::endl;
}


/*!
 * \brief Application::handleFcmReceiptMessage
 * \param id
 * \param recpt_msg
 */
void Application::handleFcmReceiptMessage(int id, const QJsonDocument& recpt_msg)
{
    std::cout << "-----------------------------------Start handleFcmReceiptMessage -----------------------------------------" << std::endl;
    PRINT_JSON_DOC(std::cout, recpt_msg);

    std::string from = recpt_msg.object().value(fcmfieldnames::FROM).toString().toStdString();
    std::string sessionid = recpt_msg.object().value(fcmfieldnames::CATEGORY).toString().toStdString();
    std::string fcm_mid = recpt_msg.object().value(fcmfieldnames::MESSAGE_ID).toString().toStdString();

    std::cout << FCM_TAG_RX(id) << "Received downstream 'receipt' from FCM, message id:"
              << fcm_mid << std::endl;

    try
    {
        SequenceId_t nextseqid = __dbConn.getNextSequenceId();

        // create gimmm message.
        QJsonDocument gimmm_msg;
        QJsonObject root;
        root[gimmmfieldnames::SEQUENCE_ID] = nextseqid;
        root[gimmmfieldnames::MESSAGE_TYPE] = "DOWNSTREAM_RECEIPT",
        root[gimmmfieldnames::SESSION_ID]   =  sessionid.c_str();
        root[gimmmfieldnames::FCM_DATA] = recpt_msg.object();
        gimmm_msg.setObject(root);
        PayloadPtr_t pmsg(new QJsonDocument(gimmm_msg));

        MessagePtr_t msgptr(new Message());
        msgptr->setSequenceId(nextseqid);
        msgptr->setType(MessageType::DOWNSTREAM_RECEIPT);
        msgptr->setFcmMessageId(fcm_mid);
        msgptr->setSourceSessionId("fcm");
        msgptr->setTargetSessionId(sessionid);
        msgptr->setPayload(pmsg);
        msgptr->setState(MessageState::NEW);

        std::cout << "New receipt message created:" << std::endl;
        std::cout << *msgptr << std::endl;

        __dbConn.saveMsg(*msgptr);
        forwardMsgToBalsession(sessionid, msgptr);
    }
    catch (std::exception& err)
    {
        PRINT_EXCEPTION_STRING(std::cout, err);
    }
    catch (...)
    {

        std::cout << "ERROR: Unknown message caught." << std::endl;
    }
    std::cout << "-----------------------------------End handleFcmReceiptMessage -------------------------------------------" << std::endl;
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
        std::cout << "Sending next downstream message with id["
                  << nextmsg->getMessageIdentifier() << "] from pending queue." << std::endl;
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
        std::cout << "Sending upstream message with id ["
                  << nextmsg->getMessageIdentifier() << "] from pending queue." << std::endl;
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
 * \brief Application::findBalMessageManager
 * \param session_id
 * \return
 */
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
 * \brief Application::forwardMsgToBalsession
 * \param session_id
 * \param msg
 */
void Application::forwardMsgToBalsession(
        const std::string& session_id,
        const MessagePtr_t& msg)
{
    std::cout << "\nStart forwarding message to BAL...." << std::endl;

    MessageManager& msgmanager = findBalMessageManager(session_id);
    msgmanager.addMessage(msg->getSequenceId(), msg);
    int rcode = msgmanager.canSendMessage(msg);
    switch (rcode)
    {
        case 0:
        {
            __dbConn.updateMsgState(*msg, MessageState::PENDING_ACK);
            msg->setState(MessageState::PENDING_ACK);
            msgmanager.incrementPendingAckCount();

            forwardMsg(session_id, msg);
            break;
        }
        case 1:
        {
            std::cout << "ERROR:Failed to forward message. Message is not in the NEW state."
                      << std::endl;
            break;
        }
        case 2:
        {
            std::cout << "WARNING:Failed to forward message. Max pending messages[" << MAX_PENDING_MESSAGES
                      << "] breached!" << std::endl;
            break;
        }
        default:
        {
            std::cout << "ERROR: Failed to foward upstream message. Unknown rcode[" << rcode << "]"
                      << std::endl;
        }
    }
}


/*!
 * \brief Application::forwardMsg
 * \param msg
 */
void Application::forwardMsg( const std::string& session_id, const MessagePtr_t& msg)
{
    std::cout << "Forwarding message with id[" << msg->getMessageIdentifier()
              << "] to sessionid [" << session_id << "]" << std::endl;

    BALSessionPtr_t sess = findBalSession(session_id);
    if (sess->getSessionState() == SessionState::UNAUTHENTICATED)
    {
        std::cout << "Session is not connected. Will try later." << std::endl;
        return;
    }

    auto gimmm_msg = msg->getPayload();
    const QJsonDocument& jdoc = *gimmm_msg;
    PRINT_JSON_DOC(std::cout, jdoc);
    try
    {
        sess->writeMessage(jdoc);
    }
    catch(std::exception& err)
    {
        PRINT_EXCEPTION_STRING(std::cout, err);
    }
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
        SequenceId_t nextseqid = __dbConn.getNextSequenceId();

        const FcmMessageId_t& msgid = msg->getFcmMessageId();
        //failure goes to the source session.
        const SessionId_t& sessid = msg->getSourceSessionId();

        QJsonDocument gimmm_msg;
        QJsonObject root;
        root[gimmmfieldnames::SEQUENCE_ID]   = nextseqid;
        root[gimmmfieldnames::MESSAGE_TYPE] = "DOWNSTREAM_REJECT";
        root[gimmmfieldnames::SESSION_ID]   = sessid.c_str();
        root[gimmmfieldnames::ERROR_DESC]   = "Max retry reached.";
        // original downstream message.
        root[gimmmfieldnames::FCM_DATA]     = msg->getPayload()->object();
        gimmm_msg.setObject(root);

        PayloadPtr_t pmsg(new QJsonDocument(gimmm_msg));
        MessagePtr_t msgptr(new Message());
        msgptr->setSequenceId(nextseqid);
        msgptr->setType(MessageType::DOWNSTREAM_REJECT);
        msgptr->setFcmMessageId(msgid);
        msgptr->setTargetSessionId(sessid);
        msgptr->setState(MessageState::PENDING_ACK);
        msgptr->setPayload(pmsg);
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
        std::cout << "ERROR: Unknown message caught." << std::endl;
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
    std::cout << "BAL_SECTION/sessions:" << std::endl;
    for (auto&& it : __balSessionMap)
    {
        BALSessionPtr_t sp = it.second;
        std::cout << "\tSESSION ID:" << sp->getSessionId() << std::endl;
    }
}


/*!
 * \brief Application::sendFcmAckMessage
 * @https://firebase.google.com/docs/cloud-messaging/server
 * For each device message your app server receives from CCS,
 * it needs to send an ACK message. It never needs to send a NACK message.
 * If you don't send an ACK for a message, CCS resends it the next time
 * a new XMPP connection is established, unless the message expires first.
 * \param json
 */
void Application::sendFcmAckMessage(const QJsonDocument& original_msg)
{
    std::string to = original_msg.object().value("from").toString().toStdString();
    std::string mid = original_msg.object().value("message_id").toString().toStdString();

    std::cout << "Sending Ack back to FCM for message with msg id[" << mid << "]..." << std::endl;

    QJsonDocument ackmsg;
    QJsonObject root;
    root[fcmfieldnames::TO] = to.c_str();
    root[fcmfieldnames::MESSAGE_ID] = mid.c_str();
    root[fcmfieldnames::MESSAGE_TYPE] = "ack";
    ackmsg.setObject(root);

    PRINT_JSON_DOC_RAW(std::cout, ackmsg);
    // ack as many and as quickly as possible.
    emit sendMessage(ackmsg);
}


/*!
 * \brief Application::retryDownstreamWithExponentialBackoff
 * \param msg
 */
void Application::retryDownstreamWithExponentialBackoff(MessagePtr_t& msg)
{
    int msec = msg->getNextRetryTimeout();
    if ( msec != -1)
    {
        msg->setRetryInProgress(true);
        auto timerCallback = [msg, this]() mutable{
            uploadToFcm(msg);
            msg->setRetryInProgress(false);
        };
        // retry sending after 'msec' millisecs.
        QTimer::singleShot(msec, Qt::TimerType::PreciseTimer, timerCallback);
    }
    else
    {
        // retry exceeded send failure.
        std::cout << "ERROR: Unable to send msg with id["
                  << msg->getSequenceId() << "]. Max retry reached."
                  << std::endl;
        __dbConn.updateMsgState(*msg, MessageState::DELIVERY_FAILED);
        __fcmMsgManager.removeMessageWithFcmMsgId(msg->getFcmMessageId());
        notifyDownstreamUploadFailure(msg);
    }
}


/*!
 * \brief Application::retryUpstreamWithExponentialBackoff
 * \param msg
 */
void Application::retryUpstreamWithExponentialBackoff(MessagePtr_t& msg)
{
    int msec = msg->getNextRetryTimeout();
    if ( msec != -1)
    {
        msg->setRetryInProgress(true);
        auto timerCallback = [msg, this]{
            forwardMsg(msg->getTargetSessionId(), msg);
            msg->setRetryInProgress(false);
        };
        // retry sending after 'msec' millisecs.
        QTimer::singleShot(msec, Qt::TimerType::PreciseTimer, timerCallback);
    }
    else
    {
        __dbConn.updateMsgState(*msg, MessageState::DELIVERY_FAILED);
        MessageManager& msgmanager = findBalMessageManager(msg->getTargetSessionId());
        msgmanager.removeMessageWithFcmMsgId(msg->getFcmMessageId());
    }
    std::cout << "ERROR: Unable to send msg [" << msg->getMessageIdentifier() << "]. Max retry reached."
              << std::endl;
}


/*!
 * \brief Application::handleBALSocketReadyRead
 */
void Application::handleBALSocketReadyRead()
{
    QTcpSocket* socket = (QTcpSocket*)sender();
    QDataStream in;
    in.setDevice(socket);

    while (socket->bytesAvailable())
    {
        QByteArray bytes;
        in.startTransaction();
        in >> bytes;
        if (!in.commitTransaction())
            return;

        std::cout << "=========================START NEW BAL MESSAGE=================================\n";
        std::cout << "Peer[" << getPeerDetail(socket) << "]" << std::endl;
        try
        {
            QJsonDocument balmsg = QJsonDocument::fromBinaryData(bytes);
            PRINT_JSON_DOC_RAW(std::cout, balmsg);
            handleBALmsg(socket, balmsg);
        }
        catch (std::exception& err)
        {
            PRINT_EXCEPTION_STRING(std::cout, err);
        }
        catch (...)
        {
          std::cout << "ERROR:Unknown exception caught." << std::endl;
        }
        std::cout << "=========================END NEW BAL MESSAGE===================================" << std::endl;
    }
}


/*!
 * \brief Application::handleBALmsg
 * \param cmd
 */
void Application::handleBALmsg(
            QTcpSocket* socket,
            const QJsonDocument& balmsg)
{
    // TODO validate jsondoc

    std::string type = balmsg.object().value(gimmmfieldnames::MESSAGE_TYPE).toString().toStdString();
    std::string session_id;


    if (type == "LOGON")
    {
         session_id  = balmsg.object().value(gimmmfieldnames::SESSION_ID).toString().toStdString();
         handleBalLogonRequest(socket, session_id);
    }
    else if (type == "DOWNSTREAM")
    {
        session_id = socket->property("session_id").toString().toStdString();
        handleBalDownstreamUploadRequest(session_id, balmsg);
    }
    else if (type == "ACK")
    {
        session_id = socket->property("session_id").toString().toStdString();
        SequenceId_t seqid = balmsg.object().value(gimmmfieldnames::SEQUENCE_ID).toInt();
        handleBalAckMsg(session_id, seqid);
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
    QTcpSocket* socket = __serverSocket.nextPendingConnection();
    connect(socket, &QAbstractSocket::readyRead, this, &Application::handleBALSocketReadyRead);
    connect(socket, &QAbstractSocket::disconnected, this, &Application::handleBALSocketDisconnected);

    quintptr id = socket->socketDescriptor();
    std::cout << "New connection with socket id:" << id << std::endl;
    BALConnPtr_t conn(new BALConn(socket));
    __balSessionMapU.emplace(id, conn);

    auto timerCallback = [conn, this]{
            handleBalAuthenticationTimeout(conn);
      };
    QTimer::singleShot(MAX_LOGON_MSG_WAIT_TIME, Qt::TimerType::PreciseTimer, conn->getTimerContext(), timerCallback);
}


/*!
 * \brief Application::handleBalAuthenticationTimeout
 * \param sess
 */
void Application:: handleBalAuthenticationTimeout(BALConnPtr_t conn)
{
    std::cout << "-----------------------------------Start handleBalAuthenticationTimeout----------------------------------------------" << std::endl;
    conn->getSocket()->blockSignals(true);
    __balSessionMapU.erase(conn->getSocket()->socketDescriptor());
    std::cout << "Peer[" << getPeerDetail(conn->getSocket())
        << "] timed out before being authenticated. Destroying connection..." << std::endl;
    std::cout << "-----------------------------------End handleBalAuthenticationTimeout--------------------------------------" << std::endl;
}


/*!
 * \brief Application::getPeerDetail
 * \param socket
 * \return
 */
std::string Application::getPeerDetail(const QTcpSocket* socket)
{
    std::stringstream ss;
    ss << "Peer name:" << socket->peerName().toStdString() <<", IP Address:"
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
        const SequenceId_t& seqid)
{
    std::cout << "----------------------------------Start handleBalAckMsg----------------------------------------\n";
    std::cout << "Received Ack for message with id[, sequenceid:" << seqid
              << "], from sessionid [" <<session_id << "]" << std::endl;
    try
    {
        MessageManager& msgmanager = findBalMessageManager(session_id);
        MessagePtr_t msg = msgmanager.findMessage(seqid);
        __dbConn.updateMsgState(*msg, MessageState::DELIVERED);
        msgmanager.removeMessage(seqid);

        sendNextPendingUpstreamMessage(msgmanager);
    }
    catch ( std::exception& err)
    {
        PRINT_EXCEPTION_STRING(std::cout, err);
    }
    catch (...)
    {

    }
    std::cout << "----------------------------------End handleBalAckMsg----------------------------------------\n";
}


/*!
 * \brief Application::handleBalLogonRequest
 * \param socket
 * \param session_id
 */
void Application::handleBalLogonRequest(
                    QTcpSocket* socket,
                    const std::string& session_id)
{
    std::cout << "----------------------------------Start handleBalLogonRequest----------------------------------------\n";
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

    std::cout << "********************NEW BAL CLIENT CONNECTED**********************" << std::endl;
    std::cout << "=        SESSION_ID:" <<  session_id << std::endl;
    std::cout << "=        PEER:"       << getPeerDetail(socket) << std::endl;
    std::cout << "=        SOCKET ID:"  << socket->socketDescriptor() << std::endl;
    std::cout << "******************************************************************" << std::endl;

    auto timerCallback = [sess, this]{
            resendPendingUpstreamMessages(sess);
      };
    QTimer::singleShot(1000, Qt::TimerType::PreciseTimer, timerCallback);
    std::cout << "----------------------------------End handleBalLogonRequest---------------------------------------" << std::endl;
}


/*!
 * \brief Application::handleBALSocketDisconnected
 */
void Application::handleBALSocketDisconnected()
{
    std::cout << "-----------------------------------Start BAL Session Disconnect-------------------------------------\n";
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
    std::cout << "----------------------------------End BAL Session Disconnect----------------------------------------\n";
}


/*!
 * \brief Application::handleBalDownstreamUploadRequest
 *  Flow control@ https://firebase.google.com/docs/cloud-messaging/server#flow
 *  "Every message sent to CCS receives either an ACK or a NACK response.
 *  Messages that haven't received one of these responses are considered pending.
 *  If the pending message count reaches 100,
 *  the app server should stop sending new messages and wait for
 *  CCS to acknowledge some of the existing pending messages"
 * \param session_id    session from where this message was recieved.
 * \param bal_downstream_msg
 */
void Application::handleBalDownstreamUploadRequest(
    const SessionId_t& session_id,
    const QJsonDocument& bal_downstream_msg)
{
    std::cout << "-----------------------------------Start handleBalDownstreamUploadRequest -------------------------------------\n";
    std::string gid  = bal_downstream_msg.object().value(gimmmfieldnames::GROUP_ID).toString().toStdString();
    QJsonObject data = bal_downstream_msg.object().value(gimmmfieldnames::FCM_DATA).toObject();
    std::string fcm_mid = data.value(fcmfieldnames::MESSAGE_ID).toString().toStdString();

    QJsonDocument jdoc(data);
    PayloadPtr_t pmsg(new QJsonDocument(jdoc));

    SequenceId_t nextseqid = __dbConn.getNextSequenceId();
    MessagePtr_t msg( new Message( nextseqid,
                                   MessageType::DOWNSTREAM,
                                   fcm_mid,
                                   gid,
                                   session_id,
                                   "fcm",
                                   pmsg));
    std::cout << "New message created:" << std::endl;
    std::cout << *msg << std::endl;

    __dbConn.saveMsg(*msg);
    __fcmMsgManager.addMessage(nextseqid, msg);
    int rcode = __fcmMsgManager.canSendMessage(msg);
    switch (rcode)
    {
        case 0:
        {
            __dbConn.updateMsgState(*msg, MessageState::PENDING_ACK);
            msg->setState(MessageState::PENDING_ACK);
            __fcmMsgManager.incrementPendingAckCount();
            uploadToFcm(msg);
            break;
        }
        case 1:
        {
            std::cout << "Failed to upload message. Message is not in the NEW state."
                      << std::endl;
            break;
        }
        case 2:
        {
            std::cout << "Failed to upload message. Max pending messages[" << MAX_PENDING_MESSAGES
                      << "] breached" << std::endl;
            break;
        }
        default:
        {
            std::cout << "Failed to upload upstream message with rcode[" << rcode << "]"
                      << std::endl;
        }
    }
    std::cout << "-----------------------------------End handleBalDownstreamUploadRequest -------------------------------------\n";
}


/*!
 * \brief Application::uploadToFcm
 * \param msg
 */
void Application::uploadToFcm(MessagePtr_t &msg)
{
    std::cout << "Uploading message with id["
              << msg->getMessageIdentifier() << "] to FCM." << std::endl;

    const QJsonDocument& jdoc = *(msg->getPayload());
    PRINT_JSON_DOC_RAW(std::cout, jdoc);
    emit sendMessage(jdoc);
}


/*!
 * \brief Application::resendPendingUpstreamMessages
 */
void Application::resendPendingUpstreamMessages(const BALSessionPtr_t& sess)
{
    std::cout << "Checking if there are any pending messages for session id["
              << sess->getSessionId() << "..." << std::endl;

    MessageManager& msgmanager  = sess->getMessageManager();
    const SessionId_t& sid      = sess->getSessionId();

    MessageQueue_t& msgmap = msgmanager.getMessages();
    std::cout << "Found [" << msgmap.size() << "] pensing messages." << std::endl;
    for (auto& it: msgmap)
    {
        MessagePtr_t msg = it.second;
        //skip all non downstream message.
        if (msg->getType() != MessageType::UPSTREAM &&
            msg->getType() != MessageType::DOWNSTREAM_ACK &&
            msg->getType() != MessageType::DOWNSTREAM_REJECT &&
            msg->getType() != MessageType::DOWNSTREAM_RECEIPT)
        {
            continue;
        }
        // resend new and pending ack messages.
        int rcode = msgmanager.canSendMessageOnReconnect(msg);
        if (rcode == 0)
        {
            std::cout << "\nResending upstream message with msgid ["
                      << msg->getMessageIdentifier() << "] to session ["
                      << sid <<"]" << std::endl;

            if ( msg->getState() != MessageState::PENDING_ACK)
            {
                __dbConn.updateMsgState(*msg, MessageState::PENDING_ACK);
                msgmanager.incrementPendingAckCount();
            }
            forwardMsg(sid, msg);
        }
        else if ( rcode == 2 )
        {
            retryUpstreamWithExponentialBackoff(msg);
        }else if (rcode == 3)
        {
            //there is another msg ahead in the same grp so lets skip for now.
            //we will attempt to resend when the next ack arrives.

            std::cout << "Cannot forward message.There are other pending message for the same groupid."
                      << std::endl;
        }
        else
        {
           std::cout << "ERROR: Unable to send message with msgid[" << msg->getMessageIdentifier()
                     << "]" << std::endl;
           msgmanager.removeMessage(msg->getSequenceId());
        }
    }
}


/*!
 * \brief Application::resendAllPendingDownstreamMessages
 * Called after a session is established/restablished with FCM. Resends all
 * NEW and PENDING ACK messages from the queue.
 *
 * REQUIREMENT:
 * Flow control @ https://firebase.google.com/docs/cloud-messaging/server#flow
 * ACKs are only valid within the context of one connection.
 * If the connection is closed before a message can be ACKed,
 * the app server should wait for CCS to resend the upstream
 * message before ACKing it again. Similarly, all pending
 * messages for which an ACK/NACK was not received from CCS
 * before the connection was closed should be sent again.
 */
void Application::resendAllPendingDownstreamMessages()
{
    std::cout << "Attempting to resend all pending downstream messages..." << std::endl;
    MessageQueue_t& msgmap = __fcmMsgManager.getMessages();
    std::cout << "Found [" << msgmap.size() << "] pending downstream messages." << std::endl;
    for (auto&& it: msgmap)
    {
        MessagePtr_t msg = it.second;

        //skip all non downstream message. Just a safety check for clumsy people.
        if (msg->getType() != MessageType::DOWNSTREAM)
        {
            std::cout << "WARNING: Non downstream message with id ["
                      << msg ->getMessageIdentifier()
                      << "] found. This shouldn't happen!" << std::endl;
            continue;
        }

        // resend new and pending ack messages.
        int rcode = __fcmMsgManager.canSendMessageOnReconnect(msg);
        switch (rcode)
        {
            case 0:
            {
                std::cout << "Resending message with msgid[" << msg->getMessageIdentifier()
                          << "] to FCM." << std::endl;

                QJsonDocument& json = *msg->getPayload();
                emit sendMessage(json);
                break;
            }
            case 2:
            {
                retryDownstreamWithExponentialBackoff(msg);
                break;
            }
            case 1:// wrong state
            case 3:// max pending message limit breached.
            default:
            {
                //do nothing
            }
        }
    }
}


/*!
 * \brief Application::hupSignalHandler
 */
void Application::hupSignalHandler(int)
{
    char a = 1;
    ::write(sigintFd[0], &a, sizeof(a));
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
 * \brief Application::handleSigInt
 */
void Application::handleSigInt()
{
  snHup->setEnabled(false);
  char tmp;
  ::read(sigintFd[1], &tmp, sizeof(tmp));

  // do Qt stuff
  std::cout << "SIGINT RECIEVED:" << std::endl;
  QCoreApplication::quit();

  snHup->setEnabled(true);
}


/*!
 * \brief Application::findBalSession
 * \param session_id
 * \return
 */
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
