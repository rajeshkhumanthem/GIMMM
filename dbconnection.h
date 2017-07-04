#ifndef DBCONNECTION_H
#define DBCONNECTION_H

#include "message.h"
#include "sqlite/sqlite3.h"

class MessageManager;

class DbConnection
{
        SequenceId_t __sequenceId;
        sqlite3* __dbhandle;
        sqlite3_stmt* __insertStmt;
        sqlite3_stmt* __updateStmt;
    public:
        DbConnection();
        ~DbConnection();
        SequenceId_t getNextSequenceId();
        void saveMsg(const Message& msg);
        void updateMsgState(const Message& msg, MessageState new_state);
        void loadPendingMessages(MessageManager& msgmanager);
    private:
        void createDb();
        void createTables();
        void createIndex();
        void readDb();
        void initSequenceId();
        void prepareStatements();
};

#endif // DBCONNECTION_H
