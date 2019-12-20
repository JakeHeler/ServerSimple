#include <signal.h>
#include <error.h>
#include <map>
#include <iostream>
#include <curl/curl.h>

#include <iostream>
#include <sstream>



#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include <cassert>
#include <cstring>

#include "my_global.h"



#include "JsonDefine.h"


#include "aes.hpp"
#include "curve25519-donna.hpp"


using namespace std;


// For encrypted data in commnication , we use the CBC method and the Diffie -Hellman key exchange
#define	ENABLE_DATA_ENCRYPTION_IN_COMMUNICATION	0
unsigned char baseKey[32]={0x00,};


enum client_command
{
  	CCOM_SLEEP, 
	CCOM_QUIT, 
	CCOM_START_NOTIFICATION, 
	CCOM_STOP_NOTIFICATION, 
	CCOM_SAVE_TOKEN, 
	CCOM_HAND_SHAKING, 
	/* Must be last */
  	CCOM_END
};



int ip_sock;


#define SOCKET_NAME    "/var/dmsocket"

pthread_mutex_t	LOCK_SetupNoti , Lockcount;

unsigned int my_port = 9877;



#include <sys/time.h>
#include <time.h>
#include <stdint.h>

static uint64_t time_now() 
{  	
	struct timeval tv;  
	uint64_t ret;	
	gettimeofday(&tv, NULL);  
	ret = tv.tv_sec;	
	ret *= 1000000;  
	ret += tv.tv_usec;  
	return ret;
}


pthread_attr_t		mConnection_attrib;
pthread_t  mThread_Id;

static const unsigned long 		TREAD_STACK_SIZE=1024*128;

static void * thread_start(void *arg);

#define MAX_CLIENT_NUMBER	1

static int indexCount[MAX_CLIENT_NUMBER];
static int thread_Count=0;


int main()
{
	
	log_init();
	log_info("Main start");

	(void) pthread_mutex_init(&LOCK_SetupNoti, NULL);
	(void) pthread_mutex_init(&Lockcount, NULL);
		

	(void) pthread_attr_init(&mConnection_attrib);

	pthread_attr_setstacksize(&mConnection_attrib, TREAD_STACK_SIZE);

	pthread_attr_setscope(&mConnection_attrib, PTHREAD_SCOPE_SYSTEM);

	//When a thread exits , this function returns its resources. Must Set . Or use the other function pthread_detach for it.
	(void) pthread_attr_setdetachstate(&mConnection_attrib, PTHREAD_CREATE_DETACHED);


	int error;
	int tryNo=MAX_CLIENT_NUMBER;
	thread_Count=0;
	
	for(int i=1 ; i <=tryNo;++i){

		indexCount[i]=i;
		if ((error = pthread_create(&mThread_Id, &mConnection_attrib, thread_start, (void*)&indexCount[i])))
		{
			// purecov: begin inspected 
			log_error("Can't create thread to handle request (error %d)", error);

			//Before go to throw , we delete resource of sem or thread att. Because destructor is not called
			pthread_attr_destroy(&mConnection_attrib);
				
			exit(0);
			
		}
		
		//sleep(1);
	}
	pthread_attr_destroy(&mConnection_attrib);

	while(1)
	{
		sleep(1);
	}
}


static std::string to_string(int value)
{
    std::stringstream strStream;
    strStream<<value;
    return strStream.str();
}

void StartAlarm(int index)
{

	char buf[2048];
	struct sockaddr_in clientaddr;
	//pthread_mutex_lock(&LOCK_SetupNoti);

	memset(buf,0x00,sizeof(buf));

	int ip_socket= socket(AF_INET, SOCK_STREAM, 0);
	if (ip_socket== 0)
	{
		cout<<"Error Socket Open"<<endl;
		exit(1);
	}

    	clientaddr.sin_family = AF_INET;
	clientaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	clientaddr.sin_port = (unsigned short) htons((unsigned short) my_port);

	if (connect(ip_socket, (struct sockaddr *)&clientaddr, sizeof(clientaddr)) < 0)
	{
	        perror("Connect error: ");
	        exit(0);
	}


#if (ENABLE_DATA_ENCRYPTION_IN_COMMUNICATION==0)

	
	char json_buf[1024]={0,};
	
	sprintf(json_buf,"{\"TOKEN\":\"Test_dk101fjAdffPA91bHZo7d%d\"}",index);



	//int len = json_data.size() +1;
	int len=0;

	for(int i=0;i<1024;i++)
	{
		if(json_buf[i] == '\0')
		{
			len =i;
			break;
		}
	}
	//cout<<len<<endl;

	// Data Format for sending data to the server
	//3bytes for data lenght
	//4th byte for category
	//5th byte for Commands
	
	len +=1; // include "null chacharacter"
	buf[0]=len & 0xFF;
	buf[1]=(len >> 8) & 0xFF;
	buf[2]=(len >> 16) & 0xFF;
	buf[3]=0;
	buf[4]=CCOM_SAVE_TOKEN;

	memcpy(&buf[5],json_buf,len);
	if(send(ip_socket, buf, len+5, MSG_NOSIGNAL ) == -1) {
		
			cout<<"First Send Date Errors="<< errno<<endl;
			close(ip_socket);
			exit(1);
	}

	//Get the resutls
	memset(buf,0x00,sizeof(buf));
	if(recv(ip_socket, buf, sizeof(buf), MSG_NOSIGNAL ) == -1)
	{
		log_info("Read	Errors=%d",errno);
		close(ip_socket);
		exit(1);	
	}
	cout<<"Return Data ="<<buf<<endl;

#else
	
	int lengthtest=32;

	uint8_t bufkey[lengthtest],pubkeys[lengthtest];
	uint8_t keyfromserver[lengthtest+1],aeskey[lengthtest];
	uint8_t basepoints[lengthtest];

	memset(aeskey,0x00,lengthtest);
	memset(bufkey,0x00,lengthtest);
	memset(basepoints,0x00,lengthtest);
	memset(pubkeys,0x00,lengthtest);
	memset(keyfromserver,0x00,lengthtest);

	
	KeyGenerator(bufkey,lengthtest);

	memcpy(basepoints,baseKey,lengthtest);
	
	curve25519_donna(pubkeys,bufkey,basepoints);
	int sendsize =lengthtest;
	
	sendsize +=1;
	buf[0]=sendsize & 0xFF;
	buf[1]=(sendsize >> 8) & 0xFF;
	buf[2]=(sendsize >> 16) & 0xFF;
	buf[3]=0;
	buf[4]=CCOM_HAND_SHAKING;//COM_START_NOTIFICATION;

	memcpy(&buf[5],pubkeys,sendsize);
	if(send(ip_socket, buf, sendsize+4, MSG_NOSIGNAL ) == -1) {
		
			cout<<"First Send Date Errors="<< errno<<endl;
			close(ip_socket);
			exit(1);
	}
	
	//Get the server key
	if(recv(ip_socket, keyfromserver, lengthtest, MSG_NOSIGNAL ) == -1)
	{
		log_info("Read	Errors=%d",errno);
		close(ip_socket);
		exit(1);	
	}

	//Make aes key for encryption or decryption
	curve25519_donna(aeskey,bufkey,keyfromserver);

    	struct AES_ctx ctx;    

	memset(buf,0x00,sizeof(buf));

	//Send Token Data
	char json_buf[1024];
	sprintf(json_buf,"{\"TOKEN\":\"Test_dk101fjAdffPA91bHZo7d%d\"}",index);

	memset(buf,0x00,sizeof(buf));

	unsigned int len=0;
	unsigned int pad_len=0;

	for(int i=0;i<1024;i++)
	{
		if(json_buf[i] == '\0')
		{
			len =i+1;
			break;
		}
	}

	char *in=PaddingDataPKCS7((unsigned char *)json_buf,len,&pad_len);

	AES_init_ctx_iv(&ctx, aeskey, &aeskey[16]);   
	AES_CBC_encrypt_buffer(&ctx, (unsigned char *)in, pad_len);

	pad_len +=1;
	buf[0]=pad_len & 0xFF;
	buf[1]=(pad_len >> 8) & 0xFF;
	buf[2]=(pad_len >> 16) & 0xFF;
	buf[3]=0;
	buf[4]=CCOM_SAVE_TOKEN;//COM_START_NOTIFICATION;

	memcpy(&buf[5],in,pad_len);

	if(send(ip_socket, buf, pad_len+4, MSG_NOSIGNAL ) == -1) {
		
			cout<<"First Send Date Errors="<< errno<<endl;
			close(ip_socket);
			exit(1);
	}
	
	free(in);


	
	//Get the resutls
	memset(buf,0x00,sizeof(buf));
	if(recv(ip_socket, buf, sizeof(buf), MSG_NOSIGNAL ) == -1)
	{
		log_info("Read	Errors=%d",errno);
		close(ip_socket);
		exit(1);	
	}

	//The data from Server has the format
	//The first 2 bytes for data lenght and the rest for data.
	int dataLen;
	dataLen =(unsigned char)(buf[0]) & 0xFF;
	dataLen +=(unsigned char)(buf[1]<<8);

	cout<<" Data Size From Server:" <<dataLen<<endl;
	AES_init_ctx_iv(&ctx, aeskey, &aeskey[16]);   
	AES_CBC_decrypt_buffer(&ctx,(unsigned char *)&buf[2],  dataLen);
	cout<<"Data:" <<&buf[2]<<endl;


#endif


	//Send Quit Command	
	memset(buf,0x00,sizeof(buf));

	buf[0]=1;
	buf[1]=0;
	buf[2]=0;
	buf[3]=0;
	buf[4]=CCOM_QUIT;

	if(send(ip_socket, buf, 1+4, MSG_NOSIGNAL ) == -1) {
				cout<<"Send Date Errors="<< errno<<endl;
				close(ip_socket);
				exit(1);
	}

	//Close
	close(ip_socket);
	//pthread_mutex_unlock(&LOCK_SetupNoti);

}

void * thread_start(void *arg)
{
	int *i = (int *) arg;
	int index =*i;
	
	//log_info("index =%d ,thread_Count=%d",index,thread_Count);
	StartAlarm(index);

	pthread_mutex_lock(&Lockcount);
	thread_Count++;
	pthread_mutex_unlock(&Lockcount);

	if(thread_Count >=MAX_CLIENT_NUMBER)
		log_info("Test Thread Finished %d", thread_Count);

	pthread_exit(0);
}





