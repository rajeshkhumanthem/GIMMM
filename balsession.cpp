#include "balsession.h"

#include <QTcpSocket>
#include <QDataStream>
#include <QJsonDocument>


/*!
 * \brief BALSession::BALSession
 * \param state
 */
BALSession::BALSession(SessionState state)
    :__state(state)
{
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
bool BALSession::writeMessage(const QJsonDocument& jsonmsg)
{
    if ( __state == SessionState::AUTHENTICATED)
    {
        QByteArray m;
        QDataStream out(&m, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_5_8);
        out << jsonmsg.toBinaryData();
        qint64 bytes = __balConn->getSocket()->write(m);
        if (bytes == -1)
            return false;
        else
            return true;
    }else
        return false;
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
