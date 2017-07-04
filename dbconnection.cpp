#include "dbconnection.h"
#include "messagemanager.h"

#include <sstream>

/*!
 * \brief set_sequenceid_callback
 * \param notused
 * \param argc
 * \param argv
 * \param colnames
 * \return
 */
static int set_sequenceid_callback(void* notused, int argc, char** argv, char** colnames )
{
    std::int64_t seqid = 0;
    if ( argc == 1)
    {
        if (argv[0] == NULL) return 0;

        std::stringstream seq;
        seq << argv[0];
        seq >> seqid;

        *((std::int64_t*)notused) = seqid;
    }
    return 0;
}


/*!
 * \brief load_pending_messages_callback
 * \param mmanager
 * \param argc
 * \param argv
 * \param colnames
 * \return
 */
static int load_pending_messages_callback(
        void* mmanager,
        int argc,
        char** argv,
        char** colnames )
{
    MessageManager& msgmanager = *((MessageManager*)mmanager);

    MessagePtr_t msg( new Message());
    for ( auto colno = 0; colno < argc; colno++)
    {
        switch(colno)
        {
            case 0:
            {
                std::stringstream r;
                r << argv[colno];

                SequenceId_t sid;
                r >> sid;

                msg->setSequenceId(sid);
                break;
            }
            case 1:
            {
                std::string datetime = argv[colno];
                msg->setEnteredDatetime(datetime);
                break;
            }
            case 2:
            {
                std::string source_sessid = argv[colno];
                msg->setSourceSessionId(source_sessid);
                break;
            }
            case 3:
            {
                std::string target_sessid = argv[colno];
                msg->setTargetSessionId(target_sessid);
                break;
            }
            case 4:
            {
                std::stringstream r;
                r << argv[colno];

                int type;
                r >> type;

                msg->setType(MessageType(type));
                break;
            }
            case 5:
            {
                std::string msgid = argv[colno];
                msg->setFcmMessageId(msgid);
                break;
            }
            case 6:
            {
                std::string gid = argv[colno]? argv[colno]: "";
                break;
            }
            case 7:
            {
                std::stringstream r;
                r << argv[colno];

                int state;
                r >> state;

                msg->setState(MessageState(state));
                break;
            }
            case 8:
            {
                std::string last_update = argv[colno];
                msg->setLastUpdateDatetime(last_update);
                break;
            }
            case 9:
            {
                QByteArray array(argv[colno]);
                QJsonDocument json = QJsonDocument::fromJson(array);
                PayloadPtr_t pay(new QJsonDocument(json));
                msg->setPayload(pay);
                break;
            }
            default:
            {
                std::cout << "ERROR: Unknown col no found:" << colno << std::endl;
            }
        }
    }
    msgmanager.addMessage(msg->getSequenceId(), msg);
    std::cout << "Loaded[\n" << *msg << "]" << std::endl;
    return 0;
}

/*!
 * \brief DbConnection::DbConnection
 * Check if table messages exists
 *     If not create it
 * else
 *      Read and initialize
 */
DbConnection::DbConnection()
{
    try
    {
        createDb();
        createTables();
        createIndex();
        prepareStatements();
        initSequenceId();
    }
    catch(std::exception& err)
    {
        sqlite3_close(__dbhandle);
        PRINT_EXCEPTION_STRING(std::cout, err);
        exit(0);
    }
}

/*!
 * \brief DbConnection::~DbConnection
 */
DbConnection::~DbConnection()
{
    sqlite3_close(__dbhandle);
    sqlite3_finalize(__insertStmt);
    sqlite3_finalize(__updateStmt);
}


/*!
 * \brief DbConnection::createDb
 */
void DbConnection::createDb()
{
    std::cout << "Creating/Opening sqlite database 'gimmmdb..." << std::endl;
    int rc = sqlite3_open("gimmmdb", &__dbhandle);
    if( rc )
    {
        std::stringstream err;
        err << "Can't open database gimmmdb. Error["
                  << sqlite3_errmsg(__dbhandle);
        THROW_INVALID_ARGUMENT_EXCEPTION(err.str());
    }
}


/*!
 * \brief DbConnection::createTables
 */
void DbConnection::createTables()
{
    std::cout << "Creating table 'messages'..." << std::endl;
    std::stringstream stmt;
    stmt << "CREATE TABLE IF NOT EXISTS messages ("
         << "sequence_id        INTEGER PRIMARY KEY, "
         << "entered_datetime   TEXT DEFAULT (datetime('now')), "
         << "source_session     TEXT NOT NULL, "
         << "target_session     TEXT NOT NULL, "
         << "type               INTEGER NOT NULL, "
         << "fcm_message_id     TEXT, "
         << "group_id           TEXT, "
         << "state              INTEGER NOT NULL, "
         << "last_update        TEXT DEFAULT (datetime('now')), "
         << "payload            TEXT NOT NULL)";

    char* errmsg;
    int rc = sqlite3_exec(__dbhandle, stmt.str().c_str(), NULL, NULL, &errmsg);
    if ( rc != SQLITE_OK)
    {
        std::stringstream err;
        err << "Cannot create table messages. Error["
                  << errmsg << "]";
        sqlite3_free(errmsg);
        THROW_INVALID_ARGUMENT_EXCEPTION(err.str());
    }
}


/*!
 * \brief DbConnection::createIndex
 */
void DbConnection::createIndex()
{
    std::cout << "Creating index on table 'messages'..." << std::endl;
    std::stringstream stmt1, stmt2;
    char* errmsg;
    int rc = SQLITE_OK;

    stmt1 << "CREATE INDEX IF NOT EXISTS idx_source_session ON messages ( target_session)";
    rc = sqlite3_exec(__dbhandle, stmt1.str().c_str(), NULL, NULL, &errmsg);
    if ( rc != SQLITE_OK)
    {
        std::stringstream err;
        err << "Cannot create index idx_source_session on table messages. Error["
                  << errmsg << "]";
        sqlite3_free(errmsg);
        THROW_INVALID_ARGUMENT_EXCEPTION(err.str());
    }

    stmt2 << "CREATE INDEX IF NOT EXISTS idx_state ON messages ( state)";
    rc = sqlite3_exec(__dbhandle, stmt2.str().c_str(), NULL, NULL, &errmsg);
    if ( rc != SQLITE_OK)
    {
        std::stringstream err;
        err << "Cannot create index idx_state on messages. Error["
                  << errmsg << "]";
        sqlite3_free(errmsg);
        THROW_INVALID_ARGUMENT_EXCEPTION(err.str());
    }

}


/*!
 * \brief DbConnection::prepareStatements
 */
void DbConnection::prepareStatements()
{
    std::stringstream insertsql;
    insertsql << "INSERT INTO messages (sequence_id, source_session, "
              << "target_session, type, fcm_message_id, group_id, state, payload) "
              <<  "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8);";

    std::cout << "insertsql:" << insertsql.str() << std::endl;

    int rc = sqlite3_prepare_v2(__dbhandle,
                      insertsql.str().c_str(),
                      -1,
                      &__insertStmt,
                      NULL);
    if ( rc != SQLITE_OK)
    {
        std::stringstream err;
        err << "Cannot prepare insert statement to table 'messages'. Error code["
                  << rc << "]";
        THROW_INVALID_ARGUMENT_EXCEPTION(err.str());
    }

    //__updateStmt;
    std::stringstream updatesql;
    updatesql << "UPDATE messages SET state = ?1, last_update = datetime('now') WHERE sequence_id = ?2";

    std::cout << "updatesql:" << updatesql.str() << std::endl;

    rc = sqlite3_prepare_v2(__dbhandle,
                      updatesql.str().c_str(),
                      -1,
                      &__updateStmt,
                      NULL);

    if ( rc != SQLITE_OK)
    {
        std::stringstream err;
        err << "Cannot prepare update statement to table 'messages'. Error code["
                  << rc << "]";
        THROW_INVALID_ARGUMENT_EXCEPTION(err.str());
    }
}


/*!
 * \brief DbConnection::saveMsg
 * \param msg
 */
void DbConnection::saveMsg(const Message &msg)
{
    sqlite3_reset(__insertStmt);
    sqlite3_clear_bindings(__insertStmt);

    sqlite3_bind_int64( __insertStmt, 1, msg.getSequenceId());
    sqlite3_bind_text ( __insertStmt, 2, msg.getSourceSessionId().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text ( __insertStmt, 3, msg.getTargetSessionId().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int  ( __insertStmt, 4, (int)msg.getType());
    sqlite3_bind_text ( __insertStmt, 5, msg.getFcmMessageId().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text ( __insertStmt, 6, msg.getGroupId().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int  ( __insertStmt, 7, (int)msg.getState());

    std::string str = msg.getPayload()->toJson(QJsonDocument::Compact).toStdString();
    sqlite3_bind_text ( __insertStmt, 8, str.c_str(), -1, SQLITE_TRANSIENT);

    // We store the json message as utf-8 string to make the col data readible from CLI tools.
    // At some point let's measure the relative performance between this approach and storing it
    // as a binary encoded message.
    //sqlite3_bind_text ( __insertStmt, 8, msg.getPayload()->toBinaryData(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(__insertStmt);
    if ( rc != SQLITE_DONE)
    {
        std::stringstream err;
        err << "Inserting message with["
            << msg.getMessageIdentifier() << "] failed. rcode[" << rc << "].";
        THROW_INVALID_ARGUMENT_EXCEPTION(err.str());
    }
}


/*!
 * \brief DbConnection::updateMsgState
 * \param msg
 * \param new_state
 */
void DbConnection::updateMsgState(const Message &msg, MessageState new_state)
{
    sqlite3_reset(__updateStmt);
    sqlite3_clear_bindings(__updateStmt);

    sqlite3_bind_int  ( __updateStmt, 1, (int) new_state);
    sqlite3_bind_int64( __updateStmt, 2, msg.getSequenceId());

    int rc = sqlite3_step(__updateStmt);
    if ( rc != SQLITE_DONE)
    {
        std::stringstream err;
        err << "Updating message with["
            << msg.getMessageIdentifier() << "] failed. rcode[" << rc << "].";
        THROW_INVALID_ARGUMENT_EXCEPTION(err.str());
    }
}


/*!
 * \brief DbConnection::initSequenceId
 */
void DbConnection::initSequenceId()
{
    //std::cout << "Initializing current sequenceid from database..." << std::endl;
    std::stringstream stmt;
    stmt << "SELECT MAX(sequence_id) FROM messages";

    __sequenceId = 0;
    char* errmsg;
    int rc = sqlite3_exec(__dbhandle, stmt.str().c_str(),
                          set_sequenceid_callback, &__sequenceId,
                          &errmsg );
    if ( rc != SQLITE_OK)
    {
        std::cout <<"ERROR: " << errmsg << std::endl;
        std::cout <<"Exiting..." << std::endl;
        exit(0);
    }
    sqlite3_free(errmsg);
    std::cout << "Sequence Id initialized to: " << __sequenceId << std::endl;
}


/*!
 * \brief DbConnection::loadPendingMessages
 * \param msgmanager
 */
void DbConnection::loadPendingMessages(MessageManager& msgmanager)
{
    //std::cout << "Initializing pending messages from database for sessionid ["
    //          << msgmanager.getSessionId() << "]..." << std::endl;

    std::stringstream stmt;
    stmt << "SELECT * FROM messages WHERE state in (1, 2) AND target_session = '"
         << msgmanager.getSessionId() << "'";

    char* errmsg;
    int rc = sqlite3_exec(__dbhandle, stmt.str().c_str(),
                          load_pending_messages_callback, &msgmanager,
                          &errmsg );
    if ( rc != SQLITE_OK)
    {
        std::cout << errmsg << std::endl;
    }
    sqlite3_free(errmsg);
}


/*!
 * \brief DbConnection::getNextSequenceId
 * \return
 */
std::int64_t DbConnection::getNextSequenceId()
{
    std::int64_t next = ++__sequenceId;
    return next;
}
