# GIMMM

C++ Implementation of the Firebase App Server (XMPP Protocol). Typical use case would be to deploy this as a standalone server. The server is intended to act as an XMPP session layer abstraction that sits nicely between your Business Application Layer and the FCM end points. The Business Application Layer is expected to be implemented as a separate process(in any language). It will simply connect to the Phantom's TCP port as specified under the 'SERVER_SECTION' in the config file to send and recieve messages. A reference implementation of a typical BAL in C++ is provided under the project GIMMCLIENT. The GIMMM daemon together with the GIMMMCLIENT forms an 'app server' as described under:

https://firebase.google.com/docs/cloud-messaging/server

By separating the 'app server' into 2 parts; a session layer (GIMMM daemon) and a business application logic aka BAL layer (GIMMM Client), an application programmer can concentrate wholely on implementing their BAL and leave the nitty gritty details of interfacing with the FCM Connection servers to the GIMMM daemon.

# Status
At this point, GIMMM is able to 

1) Establish a connection to the FCM CCS using the authentication credentials provided in the 'config.ini' file. 
2) Handle heartbeats from FCM Connection server.
3) Accepts connection from a BAL Client.
4) Recieve messages from an IOS app and forward it to the BAL Client (upstream message). 
5) Recieve messages from the BAL Client and forward it to an ios app (downstream message).
6) Flow control.
7) Handle control messages (CONNECTION DRAINING).
8) Hold on to all upstream messages if BAL client connection is down and forward to it after reconnect.

TODO:
1) Resend messages to FCM after a disconnect/reconnect.
2) Handle 'Reciepts' messages.

# XMPP Handshake with Google FCM server.
See xmpp_handshake.txt for a full list of xml messages exchanged between FCM and Phantom.

# Other Details
Implemented in C++ using QT framework.

