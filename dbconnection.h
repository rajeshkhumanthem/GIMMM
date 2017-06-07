#ifndef DBCONNECTION_H
#define DBCONNECTION_H

#include "message.h"

class DbConnection
{
        std::uint64_t __sequenceId;
        std::uint64_t __messageId;
    public:
        explicit DbConnection(std::int64_t sid = 0):__sequenceId(sid){;}
        SequenceId_t getNextSequenceId();
        MessageId_t getNextMessageId();
        void saveMsg(const Message& msg);
        void updateMsg(const Message& msg);
        void deleteMsg(const Message& msg);
};

#endif // DBCONNECTION_H
