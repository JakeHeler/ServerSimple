
# Simple Server

The Server Source is based on Mysql Server and has been simplified.
As you know , the source of mysql server is very complicated , so I modified it except that the part of source is necessary for 
general communications.
This is used for my android and IOS app ( Notifying push message to a user who set up the interval and Hour and Minute) and is tested 
enough to cover mutiple connections more than 5000 per one server.

For secure data communications , AES CBC Encrypt /Decrypt is used. For Key exchange , Diffie-Helman Method is used.
The related sources are referred to Git Hub.

*The Way to communications
1. Client connect to server by sending Client's Key Data
2. Server save the Key data and get AES Key by using the received Key Data.
   And then send Server's Key Data to Client.
3. Client get AES Key by using the received Key Data.

*Data Format in Commnunication
- Client to Server

 	-The first 3bytes for data length
  
	-4th byte for Page Number
  
	-5th byte for Commands ( Examples: Start Function, Save , Stop ...)
  
  -Real Data ( Json Format)
  
- Server to Client

  -The first 2bytes for data length
  
  -Real Data  ( Json Format)

You can check it by running Client Test .

For none Encrypt test, you can test after you change the defined contant(ENABLE_DATA_ENCRYPTION_IN_COMMUNICATION)
in server and client source.
