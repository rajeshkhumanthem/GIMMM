#include "gimmmtest.h"
#include "balsession.h"
#include "messagemanager.h"
#include "exponentialbackoff.h"

#include <QString>
void GimmmTest::initTestCase()
{

}

void GimmmTest::testExponentialBackoff()
{
    ExponentialBackoff exboff;
    QVERIFY(exboff.getMaxRetry() == NO_MAX_RETRY);
    QVERIFY(exboff.getRetry() == 0);
    QVERIFY(exboff.getSeedValue() == 2);

    int last_val = 0;
    for (int i = 1; i <= 10; i++)
    {
        int new_val = exboff.next();
        //std::cout << "New val: " << new_val << ", Last val: " << last_val << std::endl;
        QVERIFY( new_val > last_val);
        last_val = new_val;
    }
    QVERIFY(exboff.getRetry() == 10 );

    // test for backoff with max retry
    last_val = 0;
    int new_val = 0;
    ExponentialBackoff exboffMax(10);
    QVERIFY(exboffMax.getMaxRetry() == 10);
    QVERIFY(exboffMax.getRetry() == 0);
    QVERIFY(exboffMax.getSeedValue() == 2);

    for (int i = 1; i <= 10; i++)
    {
        int new_val = exboffMax.next();
        //std::cout << "New val: " << new_val << ", Last val: " << last_val << std::endl;
        QVERIFY( new_val > last_val);
        last_val = new_val;
    }
    QVERIFY(exboff.getRetry() == 10 );
    // next call should reset the val.
    new_val = exboffMax.next();
    QVERIFY( new_val < last_val);
}


void GimmmTest::testMessage()
{
    Message msg;
    QVERIFY(msg.getSequenceId() == 0);
    QVERIFY(msg.getFcmMessageId() == "");
    QVERIFY(msg.getGroupId() =="");
    QVERIFY(msg.getSourceSessionId() == "");
    QVERIFY(msg.getTargetSessionId() == "");
    QVERIFY(msg.getState() == MessageState::UNKNOWN);
    QVERIFY(msg.getType() == MessageType::UNKNOWN);
    QVERIFY(msg.getPayload().use_count() == 0);

    PayloadPtr_t payload(new QJsonDocument());

    Message msg1(1,
                 MessageType::DOWNSTREAM,
                "msgid",
                "groupid",
                "source_session_id",
                "target_session_id",
                payload);

    QVERIFY(msg1.getSequenceId() == 1);
    QVERIFY(msg1.getType() == MessageType::DOWNSTREAM);
    QVERIFY(msg1.getFcmMessageId() == "msgid");
    QVERIFY(msg1.getGroupId() =="groupid");
    QVERIFY(msg1.getSourceSessionId() == "source_session_id");
    QVERIFY(msg1.getTargetSessionId() == "target_session_id");
    QVERIFY(msg1.getState() == MessageState::NEW);
    QVERIFY(msg1.getPayload().use_count() == 3);

    msg = msg1;
    QVERIFY(msg.getSequenceId() == 1);
    QVERIFY(msg.getType() == MessageType::DOWNSTREAM);
    QVERIFY(msg.getFcmMessageId() == "msgid");
    QVERIFY(msg.getGroupId() =="groupid");
    QVERIFY(msg.getSourceSessionId() == "source_session_id");
    QVERIFY(msg.getTargetSessionId() == "target_session_id");
    QVERIFY(msg.getState() == MessageState::NEW);
    QVERIFY(msg.getPayload().use_count() == 2);
}


void GimmmTest::testGroup()
{
    Group grp("groupid");
    QVERIFY(grp.getGroupId() == "groupid");
    QVERIFY(grp.getMessageQueue().size() == 0);


    PayloadPtr_t payload1(new QJsonDocument());
    MessagePtr_t msg1( new Message(1,
                                     MessageType::DOWNSTREAM,
                                    "msgid1",
                                    "groupid",
                                    "source_session_id",
                                    "target_session_id",
                                    payload1));

    PayloadPtr_t payload2(new QJsonDocument());
    MessagePtr_t msg2(new Message(2,
                                 MessageType::DOWNSTREAM,
                                "msgid2",
                                "groupid",
                                "source_session_id",
                                "target_session_id",
                                payload2));

    grp.add(msg1);
    QVERIFY(grp.getMessageQueue().size() == 1);

    grp.add(msg2);
    QVERIFY(grp.getMessageQueue().size() == 2);

    QVERIFY(grp.canSend(msg1) == true);
    QVERIFY(grp.canSend(msg2) == false);

    grp.remove(1);
    QVERIFY(grp.getMessageQueue().size() == 1);
    QVERIFY(grp.canSend(msg2) == true);

    grp.remove(1);
    QVERIFY(grp.getMessageQueue().size() == 1);

    grp.remove(2);
    QVERIFY(grp.getMessageQueue().size() == 0);
}

void GimmmTest::testMessageManager()
{
    MessageManager msgmanager("sessionid");

    QVERIFY( msgmanager.getSessionId() == "sessionid");
    QVERIFY( msgmanager.getMaxPendingAllowed() == MAX_PENDING_MESSAGES);
    QVERIFY( msgmanager.getPendingAckCount() == 0);
    QVERIFY( msgmanager.getMessages().size() == 0);
    QVERIFY( msgmanager.getSequenceIdMap().size() == 0);
    QVERIFY( msgmanager.getMessages().size() == 0);
    QVERIFY( msgmanager.getGroupsMap().size() == 0);
}


void GimmmTest::testMessageManager_addMessage()
{
    MessageManager msgmanager("sessionid");

    PayloadPtr_t payload1(new QJsonDocument());
    MessagePtr_t msg1( new Message(1,
                                     MessageType::DOWNSTREAM,
                                    "msgid1",
                                    "groupid",
                                    "source_session_id",
                                    "target_session_id",
                                    payload1));

    PayloadPtr_t payload2(new QJsonDocument());
    MessagePtr_t msg2(new Message(2,
                                     MessageType::DOWNSTREAM,
                                    "msgid2",
                                    "groupid",
                                    "source_session_id",
                                    "target_session_id",
                                    payload2));

    PayloadPtr_t payload3(new QJsonDocument());
    MessagePtr_t msg3( new Message(3,
                                     MessageType::DOWNSTREAM,
                                    "msgid3",
                                    "groupid2",
                                    "source_session_id",
                                    "target_session_id",
                                    payload3));

    PayloadPtr_t payload4(new QJsonDocument());
    MessagePtr_t msg4( new Message(4,
                                     MessageType::DOWNSTREAM,
                                    "msgid4",
                                    "groupid",
                                    "source_session_id",
                                    "target_session_id",
                                    payload4));
    // message with no grp id
    PayloadPtr_t payload5(new QJsonDocument());
    MessagePtr_t msg5( new Message(5,
                                     MessageType::DOWNSTREAM,
                                    "msgid5",
                                    "",
                                    "source_session_id",
                                    "target_session_id",
                                    payload5));

    //msg 6: seq 6, 'groupid'
    PayloadPtr_t payload6(new QJsonDocument());
    MessagePtr_t msg6( new Message(6,
                                     MessageType::DOWNSTREAM,
                                    "msgid6",
                                    "groupid",
                                    "source_session_id",
                                    "target_session_id",
                                    payload4));


    msgmanager.addMessage(1, msg1);
    msgmanager.addMessage(2, msg2);

    QVERIFY(msgmanager.getSequenceIdMap().size() == 2);
    QVERIFY(msgmanager.getMessages().size() == 2);
    QVERIFY(msgmanager.getGroupsMap().size() == 1);
    QVERIFY(msgmanager.getPendingAckCount() == 0);

    QVERIFY(msgmanager.canSendMessage(msg1) == 0);
    QVERIFY(msgmanager.canSendMessage(msg2) != 0);

    msgmanager.addMessage(3, msg3);
    QVERIFY(msgmanager.getSequenceIdMap().size() == 3);
    QVERIFY(msgmanager.getMessages().size() == 3);
    QVERIFY(msgmanager.getGroupsMap().size() == 2);

    QVERIFY(msgmanager.canSendMessage(msg1) == 0);
    QVERIFY(msgmanager.canSendMessage(msg2) != 0);
    QVERIFY(msgmanager.canSendMessage(msg3) == 0);


    msgmanager.addMessage(4, msg4);
    QVERIFY(msgmanager.getSequenceIdMap().size() == 4);
    QVERIFY(msgmanager.getMessages().size() == 4);
    QVERIFY(msgmanager.getGroupsMap().size() == 2);

    QVERIFY(msgmanager.canSendMessage(msg1) == 0);
    QVERIFY(msgmanager.canSendMessage(msg2) != 0);
    QVERIFY(msgmanager.canSendMessage(msg3) == 0);
    QVERIFY(msgmanager.canSendMessage(msg4) != 0);


    msgmanager.addMessage(5, msg5);
    QVERIFY(msgmanager.getSequenceIdMap().size() == 5);
    QVERIFY(msgmanager.getMessages().size() == 5);
    QVERIFY(msgmanager.getGroupsMap().size() == 2);

    QVERIFY(msgmanager.canSendMessage(msg1) == 0);
    QVERIFY(msgmanager.canSendMessage(msg2) != 0);
    QVERIFY(msgmanager.canSendMessage(msg4) != 0);
    QVERIFY(msgmanager.canSendMessage(msg3) == 0);
    QVERIFY(msgmanager.canSendMessage(msg5) == 0);

    // REMOVE TEST
    msgmanager.removeMessage(1);
    QVERIFY(msgmanager.getSequenceIdMap().size() == 4);
    QVERIFY(msgmanager.getMessages().size() == 4);
    QVERIFY(msgmanager.getGroupsMap().size() == 2);

    QVERIFY(msgmanager.canSendMessage(msg2) == 0);
    QVERIFY(msgmanager.canSendMessage(msg4) != 0);
    QVERIFY(msgmanager.canSendMessage(msg3) == 0); // already in pending ack
    QVERIFY(msgmanager.canSendMessage(msg5) == 0); // already in pending ack

    msgmanager.removeMessage(2);
    QVERIFY(msgmanager.getSequenceIdMap().size() == 3);
    QVERIFY(msgmanager.getMessages().size() == 3);
    QVERIFY(msgmanager.getGroupsMap().size() == 2);

    QVERIFY(msgmanager.canSendMessage(msg4) == 0);
    QVERIFY(msgmanager.canSendMessage(msg3) == 0);
    QVERIFY(msgmanager.canSendMessage(msg5) == 0);

    msgmanager.removeMessage(4);
    QVERIFY(msgmanager.getSequenceIdMap().size() == 2);
    QVERIFY(msgmanager.getMessages().size() == 2);
    QVERIFY(msgmanager.getGroupsMap().size() == 1);

    QVERIFY(msgmanager.canSendMessage(msg3) == 0);
    QVERIFY(msgmanager.canSendMessage(msg5) == 0);

    msgmanager.addMessage(6, msg6);
    QVERIFY(msgmanager.getSequenceIdMap().size() == 3);
    QVERIFY(msgmanager.getMessages().size() == 3);
    QVERIFY(msgmanager.getGroupsMap().size() == 2);

    QVERIFY(msgmanager.canSendMessage(msg3) == 0);
    QVERIFY(msgmanager.canSendMessage(msg5) == 0);
    QVERIFY(msgmanager.canSendMessage(msg6) == 0);

    msgmanager.removeMessage(3);
    QVERIFY(msgmanager.getSequenceIdMap().size() == 2);
    QVERIFY(msgmanager.getMessages().size() == 2);
    QVERIFY(msgmanager.getGroupsMap().size() == 1);

    QVERIFY(msgmanager.canSendMessage(msg5) == 0);
    QVERIFY(msgmanager.canSendMessage(msg6) == 0);

    msgmanager.removeMessage(5);
    QVERIFY(msgmanager.getSequenceIdMap().size() == 1);
    QVERIFY(msgmanager.getMessages().size() == 1);
    QVERIFY(msgmanager.getGroupsMap().size() == 1);

    QVERIFY(msgmanager.canSendMessage(msg6) == 0);

    msgmanager.removeMessage(6);
    QVERIFY(msgmanager.getSequenceIdMap().size() == 0);
    QVERIFY(msgmanager.getMessages().size() == 0);
    QVERIFY(msgmanager.getGroupsMap().size() == 0);
}


void GimmmTest::testMessageManager_getNext()
{
    MessageManager msgmanager("sessionid");

    PayloadPtr_t payload1(new QJsonDocument());
    MessagePtr_t msg1( new Message(1,
                                     MessageType::DOWNSTREAM,
                                    "msgid1",
                                    "groupid",
                                    "source_session_id",
                                    "target_session_id",
                                    payload1));

    PayloadPtr_t payload2(new QJsonDocument());
    MessagePtr_t msg2(new Message(2,
                                     MessageType::DOWNSTREAM,
                                    "msgid2",
                                    "groupid",
                                    "source_session_id",
                                    "target_session_id",
                                    payload2));

    PayloadPtr_t payload3(new QJsonDocument());
    MessagePtr_t msg3( new Message(3,
                                     MessageType::DOWNSTREAM,
                                    "msgid3",
                                    "groupid2",
                                    "source_session_id",
                                    "target_session_id",
                                    payload3));

    PayloadPtr_t payload4(new QJsonDocument());
    MessagePtr_t msg4( new Message(4,
                                     MessageType::DOWNSTREAM,
                                    "msgid4",
                                    "groupid",
                                    "source_session_id",
                                    "target_session_id",
                                    payload4));
    // message with no grp id
    PayloadPtr_t payload5(new QJsonDocument());
    MessagePtr_t msg5( new Message(5,
                                     MessageType::DOWNSTREAM,
                                    "msgid5",
                                    "",
                                    "source_session_id",
                                    "target_session_id",
                                    payload5));

    //msg 6: seq 6, 'groupid'
    PayloadPtr_t payload6(new QJsonDocument());
    MessagePtr_t msg6( new Message(6,
                                     MessageType::DOWNSTREAM,
                                    "msgid6",
                                    "groupid",
                                    "source_session_id",
                                    "target_session_id",
                                    payload4));


    msgmanager.addMessage(1, msg1);//groupid
    msgmanager.addMessage(2, msg2);//groupid
    msgmanager.addMessage(3, msg3);//groupid2
    msgmanager.addMessage(4, msg4);//groupid
    msgmanager.addMessage(5, msg5);//""
    msgmanager.addMessage(6, msg6);//groupid

    MessagePtr_t msg;
    msg = msgmanager.getNext();
    QVERIFY(msg);
    QVERIFY(msg->getSequenceId() == 1);
    msg->setState(MessageState::PENDING_ACK);

    msg = msgmanager.getNext();
    QVERIFY(msg);
    QVERIFY(msg->getSequenceId() == 3);
    msg->setState(MessageState::PENDING_ACK);

    msg = msgmanager.getNext();
    QVERIFY(msg);
    QVERIFY(msg->getSequenceId() == 5);
    msg->setState(MessageState::PENDING_ACK);

    msg = msgmanager.getNext();
    QVERIFY(!msg);

    msgmanager.removeMessage(1);
    msg = msgmanager.getNext();
    QVERIFY(msg->getSequenceId() == 2);
    msg->setState(MessageState::PENDING_ACK);

    msgmanager.removeMessage(3);
    msg = msgmanager.getNext();
    QVERIFY(!msg);

}

void GimmmTest::testMessageManager_getPendingAckCount()
{
    MessageManager msgmanager("sessionid");

    PayloadPtr_t payload1(new QJsonDocument());
    MessagePtr_t msg1( new Message(1,
                                     MessageType::DOWNSTREAM,
                                    "msgid1",
                                    "groupid",
                                    "source_session_id",
                                    "target_session_id",
                                    payload1));

    PayloadPtr_t payload2(new QJsonDocument());
    MessagePtr_t msg2(new Message(2,
                                     MessageType::DOWNSTREAM,
                                    "msgid2",
                                    "groupid",
                                    "source_session_id",
                                    "target_session_id",
                                    payload2));

    PayloadPtr_t payload3(new QJsonDocument());
    MessagePtr_t msg3( new Message(3,
                                     MessageType::DOWNSTREAM,
                                    "msgid3",
                                    "groupid2",
                                    "source_session_id",
                                    "target_session_id",
                                    payload3));


    msgmanager.addMessage(1, msg1);
    msgmanager.addMessage(2, msg2);
    msgmanager.addMessage(3, msg3);


    QVERIFY(msgmanager.canSendMessage(msg1) == 0);
    msg1->setState(MessageState::PENDING_ACK);
    msgmanager.incrementPendingAckCount();
    QVERIFY(msgmanager.getPendingAckCount() == 1);


    QVERIFY(msgmanager.canSendMessage(msg3) == 0);
    msg3->setState(MessageState::PENDING_ACK);
    msgmanager.incrementPendingAckCount();
    QVERIFY(msgmanager.getPendingAckCount() == 2);

    // Test canSendMessage
    QVERIFY(msgmanager.canSendMessage(msg1) == 1);
    QVERIFY(msgmanager.canSendMessage(msg3) == 1);

    // Test canSendMessageOnReconnect
    QVERIFY(msgmanager.canSendMessageOnReconnect(msg1) == 0);
    QVERIFY(msgmanager.canSendMessageOnReconnect(msg3) == 0);
}


void GimmmTest::testMessageManager_findMessage()
{
    MessageManager msgmanager("sessionid");

    //test 1
    bool found = true;
    try
    {
        msgmanager.findMessage(1);
    }
    catch(std::exception& err)
    {
        found = false;
    }
    QVERIFY( found == false);


    //test 2
    PayloadPtr_t payload1(new QJsonDocument());
    MessagePtr_t msg1( new Message(1,
                                     MessageType::DOWNSTREAM,
                                    "msgid1",
                                    "groupid",
                                    "source_session_id",
                                    "target_session_id",
                                    payload1));


    found = true;
    msgmanager.addMessage(1, msg1);
    try
    {
        msgmanager.findMessage(1);
    }
    catch(std::exception& err)
    {
        found = false;
    }
    QVERIFY( found == true);

    //test 3
    found = true;
    msgmanager.removeMessage(1);
    try
    {
        msgmanager.findMessage(1);
    }
    catch(std::exception& err)
    {
        found = false;
    }
    QVERIFY( found == false);

}


void GimmmTest::testMessageManager_findMessageWithFcmMsgId()
{
    MessageManager msgmanager("sessionid");
    PayloadPtr_t payload1(new QJsonDocument());
    MessagePtr_t msg1( new Message(1,
                                     MessageType::DOWNSTREAM,
                                    "msgid1",
                                    "groupid",
                                    "source_session_id",
                                    "target_session_id",
                                    payload1));
    // test 1
    bool found = true;
    try
    {
        MessagePtr_t msg1 = msgmanager.findMessageWithFcmMsgId("msgid1");
    }
    catch(std::exception& err)
    {
        found = false;
    }
    QVERIFY( found == false);

    // test 2
    found = true;
    try
    {
        msgmanager.addMessage(1, msg1);
        MessagePtr_t msg1 = msgmanager.findMessageWithFcmMsgId("msgid1");
    }
    catch(std::exception& err)
    {
        found = false;
    }
    QVERIFY( found == true);

    //test 3
    found = true;
    try
    {
        msgmanager.removeMessage(1);
        MessagePtr_t msg1 = msgmanager.findMessageWithFcmMsgId("msgid1");
    }
    catch(std::exception& err)
    {
        found = false;
    }
    QVERIFY( found == false);
}

void GimmmTest::testMessageManager_removeMessageWithFcmMsgId()
{
    MessageManager msgmanager("sessionid");
    PayloadPtr_t payload1(new QJsonDocument());
    MessagePtr_t msg1( new Message(1,
                                     MessageType::DOWNSTREAM,
                                    "msgid1",
                                    "groupid",
                                    "source_session_id",
                                    "target_session_id",
                                    payload1));
    // test 1
    bool found = true;
    try
    {
        msgmanager.removeMessageWithFcmMsgId("msgid1");
    }
    catch(std::exception& err)
    {
        found = false;
    }
    QVERIFY( found == false);

    // test 2
    found = true;
    try
    {
        msgmanager.addMessage(1, msg1);
        msgmanager.removeMessageWithFcmMsgId("msgid1");
    }
    catch(std::exception& err)
    {
        found = false;
    }
    QVERIFY( found == true);
}
