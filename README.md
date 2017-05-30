# Phantom Server

C++ Implementation of the Firebase App Server (XMPP Protocol). Typical use case would be to deploy this as a standalone server. The server is intended to act as an XMPP session layer abstraction that sits nicely between your Business Application Layer and the FCM end points. The Business Application Layer is expected to be implemented as a separate process(in any language). It will simply connect to the Phantom's TCP port as specified under the 'SERVER_SECTION' in the config file to send and recieve messages. A reference implementation of a typical BAL will be provided in C++ will soon (followed by a python version). 

# Status
At this point, Phantom is able to establish a connection to the FCM CCS using the authentication credentials provided in the 'config.ini' file. It is heartbeating nicely. It can then send/recieve messages to/from an IOS app. However there is still a lot of pieces missing. The missing pieces will be added piecemeal as and when I find some time.

# XMPP Handshake with Google FCM server.
See xmpp_handshake.txt for a full list of xml messages exchanged between FCM and Phantom.

# Other Details
Implemented in C++ using QT framework.

