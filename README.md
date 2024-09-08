# Public-Chat-System-with-Automatic-Translation
**Project Overview**  
This project implements a Public Chat System with multiple clients. Each client uses a unique language, and messages are automatically translated so that all clients receive messages in their own language. The project demonstrates a Client-Server model and utilizes multicast and unicast communication over TCP and UDP protocols.

**Features**  
* Multiple Client Languages: Messages are automatically translated into the native language of each client.  
* Client-Server Model: Supports multiple clients connecting to a central server.  
* Multicast and Unicast Communication: Reliable message delivery is ensured using a combination of TCP (unicast) and UDP (multicast) protocols.  
* Authentication and Keep-Alive: Clients are authenticated before joining the chat, and periodic keep-alive messages are sent to maintain the connection.  
* Multicast Message Reliability: Mechanism to detect and recover from message loss in the multicast stream.


**Protocol Design**


**Message Structure**


Each message contains the following fields:  
* Type: Type of command or message.  
* ID: Unique identifier for the client.  
* Language: Language code for the client.  
* Payload Size: Size of the message payload.  
* Payload: The message content, which is automatically translated for each client.


**Key Protocol Commands**  
* Login: Client logs into the server.  
 *Join: Client confirms joining the chat and receives a multicast IP.  
* Send Message: Sends a message to the chat group.  
* Exit: Client exits the chat.  
* Keep Alive: Periodic messages to maintain the connection.  
* Multicast Indicator: Detects missing multicast messages.

  
**Testing and Demonstration**


**The project includes real-time testing scenarios such as:**  
*Authentication: Detecting an impersonation attempt.  
* Multicast Loss Handling: Closing the client socket after detecting the loss of multiple multicast messages.


**Example of a Working Session**  
* Client Login: Clients authenticate and join the chat.  
* Message Translation: Each client sends a message, and the server translates the message into the required formats (hexadecimal, binary, etc.).  
* Multicast and Unicast Messages: Messages are distributed reliably using both multicast and unicast protocols.


**Future Work**  
* Extend the system to support additional languages.  
* Enhance reliability for higher message throughput in multicast streams.  
* Explore additional security mechanisms for authentication and message encryption.


**Authors**  
Ohav Tbul  
Idan Luski
