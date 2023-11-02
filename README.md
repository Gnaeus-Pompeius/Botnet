# Botnet Server & Client

   * A simple Server & Client written in c++

## Overview

  * Decsription

  * Instructions on usage

      * How to use:
          
          * make file

          * Server

          * Client


### Description

  * Project:

    This project is a low level implementation of a Botnet Server & Client written in c++.

  * Implementation:
    
      the Server and Client use a custom library called utils. 
      The utils library is implemented in the utils.cpp file. 
      The utils library contains functionality shared and used by both the Server and Client.

### Server:

  The Server listens and accepts other Servers and clients on two different ports, 
  these ports are given as arguments when the Server is run.

  The Server accepts 6 different commands:

  * QUERYSERVERS,<FROM_GROUP_ID>

  * KEEPALIVE,<NO_OF_MESSAGS>

  * FETCH_MSG,<GROUP_ID>

  * SEND_MSG,<TO_GROUP_ID>,<FROM_GROUP_ID>,<MESSAGE_CONTENT>

  * STATUSREQ,FROM_GROUP

  * STATUSRESP,FROM_GROUP,TO_GROUP,SERVER_MESSAGES_HELD


### Client

  The Client connects to a Server through an ip address and a port, 
  the ip and port numbers are given as arguments when the Client is run.

  The Client accepts 3 different commands:

  * GETMSG,GROUP_ID
  
  * SENDMSG,GROUP_ID
  
  * LISTSERVERS


### Instructions on usage

  * Make file
  
      Firstly type the following into the terminal 
      when located inside the project directory: 

      ### make all 

  * Server
  
      To run the Server type the following into the terminal 
      when located inside the project directory:
  
      ### ./tsamgroup56 <Client port> <Server port>

  * Client
  
      To run the Server type the following into the terminal 
      when located inside the project directory:
  
      ### ./client <Ip address> <Port number>

