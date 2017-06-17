#include "fcmconnection.h"
#include "macros.h"
#include "message.h"

#include <thread>
#include <chrono>
#include <sstream>
#include <iostream>

#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>


/*!
 * \brief FcmConnection::FcmConnection
 */
FcmConnection::FcmConnection(int id)
    :__state(FcmSessionState::UNKNOWN),
     __expBoff(),
     __connectionDrainingInProgress(false),
     __id(id)
{

}


/*!
 * \brief FcmConnection::~FcmConnection
 */
FcmConnection::~FcmConnection()
{
    emit connectionShutdownStarted(__id);
    if (__fcmSocket.state() == QAbstractSocket::ConnectedState)
    {
        //Socket still open. Send STREAM END before closing connection to FCM.
        QByteArray arr("</stream:stream>");
        __fcmSocket.write(arr);
        std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_TIME_BEFORE_DISCONNECT));
        blockSignals(true);
        __fcmSocket.close();
        blockSignals(false);
    }
    emit connectionShutdownCompleted(__id);
}


/*!
 * \brief FcmConnection::connectToFcm
 * \param server_id
 * \param server_key
 * \param host
 * \param port_no
 */
void FcmConnection::connectToFcm(
                        QString server_id,
                        QString server_key,
                        QString host,
                        quint16 port_no)
{
    __fcmServerId       = server_id;
    __fcmServerKey      = server_key;
    __fcmHostAddress    = host;
    __fcmPortNo         = port_no;

    //fcm handle
    __fcmSocket.setProtocol(QSsl::TlsV1_0);
    connect(&__fcmSocket, SIGNAL(encrypted()), this, SLOT(socketEncrypted()), Qt::DirectConnection);
    connect(&__fcmSocket, SIGNAL(disconnected()), this, SLOT(handleDisconnected()), Qt::DirectConnection);
    //connect and print errors for debug. 'handleDisconnected' will be called eventually.
    connect(&__fcmSocket, static_cast<void(QSslSocket::*)(const QList<QSslError> &)>(&QSslSocket::sslErrors),
          [=](const QList<QSslError> &errors)
    {
        for ( auto&& i: errors)
        {
            emit connectionError(__id, i.errorString());
        }
    });
    __fcmWriter.setDevice(&__fcmSocket);
    connectToFirebase();
}


/*!
 * \brief FcmConnection::connectToFirebase
 */
void FcmConnection::connectToFirebase()
{
    //std::cout << "Connecting to FCM server..." << std::endl;
    emit connectionStarted(__id);
    __fcmSocket.connectToHostEncrypted(__fcmHostAddress, __fcmPortNo);
}


/*!
 * \brief FcmConnection::socketEncrypted
 */
void FcmConnection::socketEncrypted()
{
    emit connectionEstablished(__id);
    //std::cout << "Connected to FCM server.Starting XMPP handshake. Opening stream..." << std::endl;

    //socket is secured now, lets prepare to read messages.
    connect(&__fcmSocket, SIGNAL(readyRead()), this, SLOT(handleReadyRead()), Qt::DirectConnection);

    emit xmppHandshakeStarted(__id);
    __fcmWriter.writeStartDocument();
    __fcmWriter.writeStartElement("stream:stream");
    __fcmWriter.writeAttribute("to", "gcm.googleapis.com");
    __fcmWriter.writeAttribute("version", "1.0");
    __fcmWriter.writeAttribute("xmlns", "jabber:client");
    __fcmWriter.writeAttribute("xmlns:stream", "http://etherx.jabber.org/streams");

    QByteArray arr(">");// Do we need to escape this??
    __fcmSocket.write(arr);
}


/*!
 * \brief FcmConnection::handleDisconnected
 *        If the disconnect arrives because of a connection draining in progress
 *        then emit 'connectionDrainingCompleted' otherwise emit 'connectionLost'
 *        and reconnect.
 */
void FcmConnection::handleDisconnected()
{
    __state = FcmSessionState::UNKNOWN;
    if ( __connectionDrainingInProgress == false)
    {
        emit connectionLost(__id);
        int msec = __expBoff.next();
        QTimer::singleShot(msec, this, &FcmConnection::connectToFirebase);
    }else
    {
        emit connectionDrainingCompleted(__id);
    }
}


/*!
 * \brief FcmConnection::handleReadyRead
 */
void FcmConnection::handleReadyRead()
{
    QSslSocket* socket = (QSslSocket*)sender();
    QByteArray bytes;
    bytes = socket->readAll();

    if (bytes.length() == 1 && isspace(bytes.at(0)))
    {
        //std::cout << "RX[1]: Recieved keepalive from FCM." << std::endl;
        emit heartbeatRecieved(__id);
        return;
    }
    // Recieved regular message. We can start parsing.
    // TODO implement an option to print this to a file.
    //std::cout << std::endl << "RX["<< bytes.length() <<"]:" << bytes.toStdString() << std::endl;
    __fcmReader.addData(bytes);
    try
    {
        parseXml();
    }
    catch(std::exception& err)
    {
        //std::cout << "Exception caught. Error[" << err.what() << std::endl;
        emit connectionError(__id, err.what());
    }
    catch (...)
    {
        std::stringstream err;
        err << "unknown exception caught.";
        emit connectionError(__id, err.str().c_str());
    }
}


/*!
 * \brief FcmConnection::parseXml
 */
void FcmConnection::parseXml()
{
    //std::cout << "Parsing xml..." << std::endl;
    while (__fcmReader.readNextStartElement())
    {
        handleStartElement();
    }

    // check if we got an end of stream stanza from FCM.
    handleOtherElement();

    // Log if there was an unexpected error.
    if (__fcmReader.error())
    {
        if (__fcmReader.error() != QXmlStreamReader::PrematureEndOfDocumentError)
        {
            //std::cout << __fcmReader.errorString().toStdString() << std::endl;
            emit connectionError(__id, __fcmReader.errorString());
        }
    }
}


/*!
 * \brief FcmConnection::handleOtherElement
 */
void FcmConnection::handleOtherElement()
{
    while (!__fcmReader.atEnd())
    {
        __fcmReader.readNext();
        if (__fcmReader.isEndDocument())
        {
            handleEndOfStream();
        }
    }
}


/*!
 * \brief FcmConnection::handleEndOfStream
 * Section 4.4 RFC 6120
 */
void FcmConnection::handleEndOfStream()
{
    //std::cout << "Recieved 'END STREAM' from FCM. Closing stream..." << std::endl;
    __fcmWriter.writeEndElement();
    __fcmWriter.writeEndDocument();
    emit streamClosed(__id);
}


/*!
 * \brief FcmConnection::handleStartElement
 */
void FcmConnection::handleStartElement()
{
    if (__fcmReader.name() == "stream" && __fcmReader.namespaceUri() == STREAM_NSPACE_URI )
    {
        handleStartStream();
    }
    else if (__fcmReader.name() == "features" && __fcmReader.namespaceUri() == STREAM_NSPACE_URI)
    {
        handleFeatures();
    }
    else if (__fcmReader.name() == "iq" && __fcmReader.attributes().value("type") == "result" &&
                 __fcmReader.namespaceUri() == DEFAULT_NSPACE_URI)
    {
        handleIq();
    }
    else if (__fcmReader.name() == "success" && __fcmReader.namespaceUri() == SASL_NSPACE_URI)
    {
        handleSaslSuccess();
    }
    else if (__fcmReader.name() == "failure" && __fcmReader.namespaceUri() ==  SASL_NSPACE_URI)
    {
        handleSaslFailure();
    }
    else if (__fcmReader.name() == "message" && __fcmReader.namespaceUri() == DEFAULT_NSPACE_URI)
    {
        handleMessage();
    }
    else
    {
            std::stringstream err;
            err << "ERROR:Unknown stanza recieved from FCM, name:["
                      << __fcmReader.name().toString().toStdString() << "]";
            THROW_INVALID_ARGUMENT_EXCEPTION(err.str());
    }
}


/*!
 * \brief FcmConnection::handleStartStream
 */
void FcmConnection::handleStartStream()
{
    /*
    std::cout << "Recieved new stream header from FCM, id:"
              << __fcmReader.attributes().value("id").toString().toStdString()
              << std::endl;
              */
}


/*!
 * \brief FcmConnection::handleFeatures
 */
void FcmConnection::handleFeatures()
{
    //std::cout << "Recieved 'features' stanza from FCM server." << std::endl;
    while (__fcmReader.readNextStartElement())
    {
          if (__fcmReader.name() == "mechanisms" && __fcmReader.namespaceUri() == SASL_NSPACE_URI)
              readMechanisms();
          else if (__fcmReader.name() == "bind" && __fcmReader.namespaceUri() == BIND_NSPACE_URI)
              readBind();
          else if (__fcmReader.name() == "session" && __fcmReader.namespaceUri() == SESSION_NSPACE_URI)
              readSession();
          else
              __fcmReader.skipCurrentElement();
    }
}


/*!
 * \brief FcmConnection::readMechanisms
 */
void FcmConnection::readMechanisms()
{
    //std::cout << "	Recieved supported SASL auth mechanism list from FCM server." << std::endl;
    while (__fcmReader.readNextStartElement()) {
          if (__fcmReader.name() == "mechanism")
              readMechanism();
          else
              __fcmReader.skipCurrentElement();
    }
    QTimer::singleShot(10, this, &FcmConnection::sendAuthenticationInfo);
}


/*!
 * \brief FcmConnection::readMechanism
 */
void FcmConnection::readMechanism()
{
    std::string method = __fcmReader.readElementText().toStdString();
   // std::cout << "		Recieved sasl auth method:" << method << std::endl;
    __authMethodVect.push_back(method);
}


/*!
 * \brief FcmConnection::readBind
 */
void FcmConnection::readBind()
{
    __fcmReader.readElementText();
    //std::cout << "	Recieved xmpp 'bind' feature from FCM server." << std::endl;
    QTimer::singleShot(10, this, &FcmConnection::sendIQBind);
}


/*!
 * \brief FcmConnection::readSession
 */
void FcmConnection::readSession()
{
    __fcmReader.readElementText();
    //std::cout << "	Recieved xmpp 'session' feature from FCM server." << std::endl;
}


/*!
 * \brief FcmConnection::handleSaslSuccess
 * 4.3.3 On successful negotiation of a feature that necessitates a stream
 * restart, both parties MUST consider the previous stream to be
 * replaced but MUST NOT send a closing </stream> tag and MUST NOT
 * terminate the underlying TCP connection; instead, the parties MUST
 * reuse the existing connection, which might be in a new state (e.g.,
 * encrypted as a result of TLS negotiation).  The initiating entity
 * then MUST send a new initial stream header, which SHOULD be preceded
 * by an XML declaration as described under Section 11.5.  When the
 * receiving entity receives the new initial stream header, it MUST
 * generate a new stream ID (instead of reusing the old stream ID)
 * before sending a new response stream header (which SHOULD be preceded
 * by an XML declaration as described under Section 11.5).
 */
void FcmConnection::handleSaslSuccess()
{
    __fcmReader.readElementText();
    //std::cout << "Recieved xmpp-sasl SUCCESS from FCM server." << std::endl;
    emit saslSucess(__id);
    QTimer::singleShot(10, this, &FcmConnection::startNewStream);
}


/*!
 * \brief FcmConnection::handleSaslFailure
 */
void FcmConnection::handleSaslFailure()
{
    __fcmReader.readNextStartElement();
    QString failureReason = __fcmReader.name().toString();
    std::stringstream err;
    err << "Recieved xmpp-sasl FAILURE from FCM server. Reason:"
              << failureReason.toStdString() << std::endl;
    emit connectionError(__id, err.str().c_str());

    // TODO try another authentication method ??
}


/*!
 * \brief FcmConnection::handleIq
 */
void FcmConnection::handleIq()
{
    //std::cout << "Recieved IQ stanza from FCM server." << std::endl;
    //std::cout << "Bind attr:" << __fcmReader.attributes().value("type").toString().toStdString() << std::endl;
    while (__fcmReader.readNextStartElement())
    {
          if (__fcmReader.name() == "bind" &&
              __fcmReader.namespaceUri() == BIND_NSPACE_URI)
          {
              readIQBindResult();
          }
          else
              __fcmReader.skipCurrentElement();
    }
}


/*!
 * \brief FcmConnection::readIQBindResult
 */
void FcmConnection::readIQBindResult()
{
    //std::cout << "Recieved IQ Bind result from FCM server." << std::endl;
    while (__fcmReader.readNextStartElement()) {
          if (__fcmReader.name() == "jid")
              readJid();
          else
              __fcmReader.skipCurrentElement();
    }
}


/*!
 * \brief FcmConnection::readJid
 */
void FcmConnection::readJid()
{
    //std::string jid = __fcmReader.readElementText().toStdString();
    //std::cout << "Recieved JID from FCM server:"<< jid << std::endl;

    __state = FcmSessionState::AUTHENTICATED;
    __expBoff.resetRetry();
    emit sessionEstablished(__id);
}


/*!
 * \brief FcmConnection::handleMessage
 */
void FcmConnection::handleMessage()
{
    //std::cout << "Parsing new XMPP stanza..." << std::endl;
    while (__fcmReader.readNextStartElement())
    {
          if (__fcmReader.name() == "gcm" && __fcmReader.namespaceUri() == GCM_NSPACE_URI)
          {
              QString json = __fcmReader.readElementText();
              handleFcmMessage(json);
          }
          else
              __fcmReader.skipCurrentElement();
    }
}


/*!
 * \brief FcmConnection::handleFcmMessage
 * \param json_str
 */
void FcmConnection::handleFcmMessage(const QString& json_str)
{
    //std::cout << "-------------------START NEW FCM MESSAGE ----------------------" << std::endl;
    QByteArray bytes(json_str.toStdString().c_str());

    QJsonParseError parseErr;
    QJsonDocument jdoc = QJsonDocument::fromJson(bytes, &parseErr);
    if (jdoc.isNull())
    {
        std::stringstream err;
        err << "ERROR: Failed to parse recieved msg, error["
                  << parseErr.errorString().toStdString()<< "]";
        emit connectionError(__id, err.str().c_str());
        return;
    }
    //std::cout << "Message:" << std::endl;
    //PRINT_JSON_DOC(std::cout, jdoc);

    // All good so far.
    if ( jdoc.object().contains("message_type"))
    {
        // this is either an 'ack'/'nack'/'control'
        std::string msg_type = jdoc.object().value(fcmfieldnames::MESSAGE_TYPE).toString().toStdString();
        if ( msg_type == "ack")
        {
            emit newAckMessage(__id, jdoc);
        }
        else if ( msg_type == "nack")
        {
            emit newNackMessage(__id, jdoc);
        }
        else if ( msg_type == "control")
        {
            handleControlMessage(jdoc);
        }else if (msg_type == "receipt")
        {
            handleReceiptMessage(jdoc);
        }else
        {
            std::stringstream err;
            err << "Unknown message type<" << msg_type << "> found.";
            THROW_INVALID_ARGUMENT_EXCEPTION(err.str());
        }
    }else
    {
        // this is a new message from a client.
        emit newMessage(__id, jdoc);
    }
    //std::cout << "-------------------END NEW FCM MESSAGE ----------------------" << std::endl;
}


/*!
 * \brief Application::handleControlMessage
 * Control messages @ https://firebase.google.com/docs/cloud-messaging/server
 *
 * Periodically, CCS needs to close down a connection to perform load balancing.
 * Before it closes the connection, CCS sends a CONNECTION_DRAINING message to
 * indicate that the connection is being drained and will be closed soon.
 * "Draining" refers to shutting off the flow of messages coming into a connection,
 * but allowing whatever is already in the pipeline to continue. When you receive
 * a CONNECTION_DRAINING message, you should immediately begin sending messages to
 * another CCS connection, opening a new connection if necessary. You should,
 * however, keep the original connection open and continue receiving messages that
 * may come over the connection (and ACKing them)—CCS handles initiating a connection
 * close when it is ready.
 * The CONNECTION_DRAINING message looks like this:
 *
 *       <message>
 *         <data:gcm xmlns:data="google:mobile:data">
 *         {
 *           "message_type":"control"
 *           "control_type":"CONNECTION_DRAINING"
 *         }
 *         </data:gcm>
 *       </message>
 *       CONNECTION_DRAINING is currently the only control_type supported.
 *
 * \param control_msg
 */
void FcmConnection::handleControlMessage(const QJsonDocument& control_msg)
{
    //std::cout << "Recieved 'control' message from FCM..." << std::endl;
    QString control_type = control_msg.object().value(fcmfieldnames::CONTROL_TYPE).toString();
    if ( control_type == "CONNECTION_DRAINING" )
    {
        __connectionDrainingInProgress = true;
        emit connectionDrainingStarted(__id);
    }
    else
    {
        std::stringstream err;
        err << "Unknown control message recieved from FCM.";
        QString qerr(err.str().c_str());
        emit connectionError(__id, qerr);
    }
}


/*!
 * \brief FcmConnection::handleReceiptMessage
 * \param receipt_msg
 */
void FcmConnection::handleReceiptMessage(const QJsonDocument& receipt_msg)
{
    emit newReceiptMessage(__id, receipt_msg);
}


/*!
 * \brief FcmConnection::sendAuthenticationInfo
 */
void FcmConnection::sendAuthenticationInfo()
{
    //std::cout << std::endl << "TX:Sending authen info to FCM..." << std::endl;

    __fcmWriter.writeStartElement("auth");
    __fcmWriter.writeAttribute("mechanism", "PLAIN");
    __fcmWriter.writeAttribute("xmlns","urn:ietf:params:xml:ns:xmpp-sasl");

    QByteArray authenString(__fcmServerId.toStdString().c_str());
    authenString.append("@gcm.googleapis.com");

    authenString.insert(0, '\0');
    int size = authenString.size();
    authenString.insert(size, '\0');

    QByteArray passwd(__fcmServerKey.toStdString().c_str());
    size = authenString.size();
    authenString.insert(size, passwd);

    __fcmWriter.writeCharacters(authenString.toBase64().data());
    __fcmWriter.writeEndElement();
}


/*!
 * \brief FcmConnection::startNewStream
 */
void FcmConnection::startNewStream()
{
    //std::cout << std::endl << "TX:Starting new stream...." << std::endl;
    __fcmWriter.writeStartElement("stream:stream");
    __fcmWriter.writeAttribute("to", "gcm.googleapis.com");
    __fcmWriter.writeAttribute("version", "1.0");
    __fcmWriter.writeAttribute("xmlns", "jabber:client");
    __fcmWriter.writeAttribute("xmlns:stream", "http://etherx.jabber.org/streams");

    // QT stream reader doesn't let me write '>'. So let's write it directly to
    // the socket.
    QByteArray arr(">");
    __fcmSocket.write(arr);
    emit streamOpened(__id);
}


/*!
 * \brief FcmConnection::sendIQBind
 */
void FcmConnection::sendIQBind()
{
    //std::cout << std::endl << "TX:Sending IQ bind to FCM..." << std::endl;
    __fcmWriter.writeStartElement("iq");
    __fcmWriter.writeAttribute("type","set");
    __fcmWriter.writeStartElement("bind");
    __fcmWriter.writeAttribute("xmlns", "urn:ietf:params:xml:ns:xmpp-bind");
    __fcmWriter.writeEndElement();
    __fcmWriter.writeEndElement();
}


/*!
 * \brief Application::sendMessage
 * \param data - This is a fully formed FCM message.
 */
void FcmConnection::handleSendMessage(const QJsonDocument &data)
{
    //std::cout << "Sending Message to FCM:" << std::endl;
    //PRINT_JSON_DOC(std::cout, data);
    __fcmWriter.writeStartElement("message");
    __fcmWriter.writeAttribute("id", "");
    __fcmWriter.writeStartElement("gcm");
    __fcmWriter.writeAttribute("xmlns", GCM_NSPACE_URI);
    __fcmWriter.writeCharacters(data.toJson());
    __fcmWriter.writeEndElement();
    __fcmWriter.writeEndElement();
}
