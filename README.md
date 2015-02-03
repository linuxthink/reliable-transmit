# reliable-transmit
Reliable transmission over noisy UDP channel 

(UIUC CS438 Spring 2013 MP3, Member: Tao Li, Yixing Lao)

1. References
=============
1. Man pages of UDP related function.
2. Hash function from
   <a href="http://www.azillionmonkeys.com/qed/hash.html">
   Super Fast Hash</a>


2. General Architecture
=======================

(1) Callback list
-----------------
* When triggered by an event (e.g. sending a packet), the sender
  will register a callback event in the callback list with
  callback time.
* The event_loop check whether there is a callback timeout by
  comparing current time with the event callback time, if current
  time go pass the event callback time, then it will trigger the
  timeout event by calling the callback function
* The event_loop will also monitor the UDP socket and listen to
  incoming packets, after checking its integrity, the event_loop
  will trigger the corresponding event related to that incoming
  packets.

(2) Packet format design
------------------------
* There are 4 kinds of packets:
  1. PACKET\_TYPE\_INIT: for carrying handshake packets
  2. PACKET\_TYPE\_DATA: for carrying data packets
  3. PACKET\_TYPE\_ACK: for carrying ack packets
  4. PACKET\_TYPE\_BYE: for carrying terminating packets
* The header size of the packet is 12 bytes
  1. type: 2 bytes
  2. packet_length: 2 bytes
  3. sequence\_no: 4 bytes
  4. checksum: 4 bytes
* Also we have special function for encoding and extracting
      packets, as well as checking the integrity of the packets

(3) States of sender and receiver
---------------------------------
* STATE\_INIT: for handshake
* STATE\_DATA: for sending data packets
* STATE\_BYE: for terminating the connection

(4) Sliding window implementation
---------------------------------
* The sender's sliding window is registered in the callback list.
  Each entry of the callback list contains information about the
  sequence number, callback time and a enable flag.
* The program will keep checking the callback list to see whether
  there is a callback event, if there is a callback timeout, the
  program will execute the callback function.
* For simplicity, we use constant window size in this MP.


3. Testing Results
==================

We have tested our program for performance and correctness. In all
our testings, the program performs reliably, with the file correctly
transfered. The following is the running time for the performance
(examples).

lena.bmp (768.1 KB)
-------------------

1. * sender: setMP3Params(1000, 1, 10, 0.0, 0.0);
   * receiver: setMP3Params(1000, 1, 10, 0.0, 0.0);
   * running time (sender): 139ms

2. * sender: setMP3Params(1000, 1, 10, 0.2, 0.2);
   * receiver: setMP3Params(1000, 1, 10, 0.2, 0.2);
   * running time (sender): 165ms

3. * sender: setMP3Params(1000, 1, 10, 0.4, 0.4);
   * receiver: setMP3Params(1000, 1, 10, 0.4, 0.4);
   * running time (sender): 229ms

4. * sender: setMP3Params(1000, 1, 10, 0.6, 0.6);
   * receiver: setMP3Params(1000, 1, 10, 0.6, 0.6);
   * running time (sender): 615ms

5. * sender: setMP3Params(1000, 1, 10, 0.8, 0.8);
   * receiver: setMP3Params(1000, 1, 10, 0.8, 0.8);
   * running time (sender): 6924ms
