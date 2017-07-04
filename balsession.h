#ifndef BALSESSION_H
#define BALSESSION_H

#include "messagemanager.h"

#include <QObject>

#include <iostream>
#include <memory>
#include <vector>

#include <QTcpSocket>


/*!
 * \brief The SessionState enum
 */
enum class SessionState: char
{
    UNAUTHENTICATED = 'U',
    AUTHENTICATED = 'A'
};


class QTcpSocket;
class QJsonDocument;
class BALConn
{
        QTcpSocket*       __socket;
        QObject*          __timeoutContext;
    public:
        BALConn(QTcpSocket* socket) :__socket(socket), __timeoutContext(new QObject())
        { std::cout << "New BALCONN created. this:" << this << ", socket:" << __socket->socketDescriptor() << std::endl;}
        ~BALConn()
        {
            std::cout << "DESTROYED BALCONN, this:" << this << ", socket:" << __socket->socketDescriptor() <<  std::endl;
            if (__timeoutContext){ delete __timeoutContext; __timeoutContext = NULL;}
            if (__socket) __socket->deleteLater();
        }
        QObject*            getTimerContext() const { return __timeoutContext;}
        QTcpSocket*         getSocket() const { return __socket;}
        void                deleteTimerContext(){ delete __timeoutContext; __timeoutContext = NULL;}
};

/*!
 * \brief BALConnPtr_t
 */
typedef std::shared_ptr<BALConn> BALConnPtr_t;


/*!
 * \brief The BALSession class
 */
class BALSession
{
      BALConnPtr_t      __balConn;
      std::string       __sessionId; // BAL identifir; category field in FCM.
      SessionState      __state;
      MessageManager    __msgManager;

      friend std::ostream &operator<< (std::ostream&, const BALSession&);
    public:
        BALSession(const std::string& sessid,
                   SessionState state = SessionState::UNAUTHENTICATED);
        ~BALSession();
        void                disconnectFromHost();
        void                writeMessage(const QJsonDocument& jsonmsg);
        void                setSessionId(const std::string& sid) { __sessionId = sid;}
        void                setState( SessionState state) { __state = state;}
        void                setConn(BALConnPtr_t con) { __balConn = con;}

        const std::string&  getSessionId() const {return __sessionId;}
        SessionState        getSessionState() const { return __state;}
        BALConnPtr_t        getConn() const {return __balConn;}
        MessageManager&     getMessageManager() {return __msgManager;}
};

/*!
 * \brief operator <<
 * \param output
 * \param rhs
 * \return
 */
inline std::ostream &operator<<( std::ostream& output, const BALSession& rhs)
{
      output << "SessionID:" << rhs.getSessionId();
      return output;
}

/*!
 * \brief BALSessionPtr_t
 */
typedef std::shared_ptr<BALSession> BALSessionPtr_t;


#endif // BALSESSION_H

