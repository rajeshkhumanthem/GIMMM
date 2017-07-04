#ifndef GIMMMTEST_H
#define GIMMMTEST_H


#include <QObject>
#include <QtTest/QtTest>

class GimmmTest: public QObject
{
        Q_OBJECT
    private slots:
        void initTestCase();
        void testExponentialBackoff();
        void testMessage();
        void testGroup();
        void testMessageManager();
        void testMessageManager_addMessage();
        void testMessageManager_getPendingAckCount();
        void testMessageManager_findMessage();
        void testMessageManager_findMessageWithFcmMsgId();
        void testMessageManager_removeMessageWithFcmMsgId();
        void testMessageManager_getNext();
};

#endif // GIMMMTEST_H
