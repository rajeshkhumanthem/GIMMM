# GIMMM

C++ Implementation of the Firebase App Server (XMPP Protocol). Typical use case would be to deploy this as a standalone server. The server is intended to act as an XMPP session layer abstraction that sits nicely between your Business Application Layer and the FCM end points. The Business Application Layer is expected to be implemented as a separate process(in any language). It will simply connect to the TCP port as specified under the 'SERVER_SECTION' in the config file to send and recieve messages. A reference implementation of a typical BAL in C++ is provided under the project GIMMMCLIENT. The GIMMM daemon together with the GIMMMCLIENT forms an 'app server' as described under:

https://firebase.google.com/docs/cloud-messaging/server

By separating the 'app server' into 2 parts; a session layer (GIMMM daemon) and a business application logic aka BAL layer (GIMMM Client), an application programmer can concentrate wholely on implementing their BAL and leave the nitty gritty details of interfacing with the FCM Connection servers to the GIMMM daemon.

For a more detail documentation please see [Gimmm Wiki Page](https://github.com/rajeshkhumanthem/GIMMM/wiki).

# Status
At this point, GIMMM is able to 

1) Establish a connection to the FCM CCS using the authentication credentials provided in the 'config.ini' file. 
2) Handle heartbeats from FCM Connection server.
3) Handle ack/nack from the FCM Connection server.
4) Handle control messages (CONNECTION DRAINING).
5) Retry with exponential backoff. 
6) Flow control.
7) Recieve messages from an IOS app and forward it to the BAL Client (upstream message). 
8) Recieve messages from the BAL Client and forward it to an ios app (downstream message).  
9) Hold on to all upstream messages if BAL client connection is down and forward to it after a successfull reconnect.
10) Resend messages to FCM after a disconnect/reconnect.
TODO:

2) Handle 'Reciepts' messages (in progress).
3) Quick start guide (in progress). 
4) Reference implementation of BAL in other languages like python, java etc.
5) Persist message using Sqlite. To persist upstream/downstream message for resend.
6) Implement message grouping for strict order messaging (done).
7) Utility class for message id generation.

# Other Details
Implemented in C++ using QT framework.

# Open issues
1. From FCM documentation: "Sending an ACK message
In response to an upstream message like the above, the app server must use the same connection to send
an ACK message containing the unique message ID. If FCM does not receive an ACK,
it may retry sending the message to the app server." - How quickly do we need to send the ack message?


