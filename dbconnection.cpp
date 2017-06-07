#include "dbconnection.h"

#include <sstream>

void DbConnection::saveMsg(const Message &msg)
{

}

void DbConnection::updateMsg(const Message &msg)
{

}

void DbConnection::deleteMsg(const Message &msg)
{

}

SequenceId_t DbConnection::getNextSequenceId()
{
    return ++__sequenceId;
}

MessageId_t DbConnection::getNextMessageId()
{
    std::uint64_t msgid = ++__messageId;
    std::stringstream out;
    out << msgid;
    return out.str();
}
