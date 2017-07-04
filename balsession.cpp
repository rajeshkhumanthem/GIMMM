#include "balsession.h"
#include "macros.h"

#include <sstream>

#include <QTcpSocket>
#include <QDataStream>
#include <QJsonDocument>


/*!
 * \brief BALSession::BALSession
 * \param state
 */
BALSession::BALSession(const std::string& sessid,
                       SessionState state)
    :__sessionId(sessid),
     __state(state),
     __msgManager(sessid)
{
    //TODO initialize messagemanager with unsent messages.
}


/*!
 * \brief BALSession::~BALSession
 */
BALSession::~BALSession()
{
}


/*!
 * \brief BALSession::writeMessage
 * \param jsonmsg
 * \return
 */
void BALSession::writeMessage(const QJsonDocument& jsonmsg)
{
    //std::cout << "Printing json..." << std::endl;
    //PRINT_JSON_DOC(std::cout, jsonmsg);
    if ( __state == SessionState::AUTHENTICATED)
    {
        QByteArray m;
        QDataStream out(&m, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_5_8);
        out << jsonmsg.toBinaryData();
        qint64 bytes = __balConn->getSocket()->write(m);
        if (bytes == -1)
        {
            std::stringstream err;
            err << "ERROR: Failed t write to session id:" << __sessionId;
            THROW_INVALID_ARGUMENT_EXCEPTION(err.str());
        }
    }
    else
    {
        std::stringstream err;
        err << "Failed t write to session id:"
            << __sessionId << ", Invalid session state:"
            << (char)__state;
        THROW_INVALID_ARGUMENT_EXCEPTION(err.str());
    }
}


/*!
 * \brief BALSession::disconnectFromHost
 */
void BALSession::disconnectFromHost()
{
    std::cout << "Disconnecting ......" << std::endl;
    if (__state == SessionState::AUTHENTICATED)
    {
        std::cout << "Disconnecting authenticated conn ......" << std::endl;
        __balConn.reset();
    }
}
