#include "balsession.h"

#include <QTcpSocket>
#include <QDataStream>
#include <QJsonDocument>


#define TEST_MODE

#ifdef TEST_MODE
#include "macros.h"
#endif

/*!
 * \brief BALSession::BALSession
 * \param state
 */
BALSession::BALSession(SessionState state)
    :__state(state)
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
bool BALSession::writeMessage(const QJsonDocument& jsonmsg)
{
    std::cout << "Printing json..." << std::endl;
    PRINT_JSON_DOC(std::cout, jsonmsg);
    return true;
    /*
#else
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
#endif
*/
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
