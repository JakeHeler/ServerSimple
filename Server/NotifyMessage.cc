#include "./json/document.h"
#include "json/stringbuffer.h"
#include "json/writer.h"

#include "NotifyMessage.h"
#include "log.h"

#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <stdio.h>
#include <iostream>
#include <sstream>

#include <curl/curl.h>
#include <dlfcn.h>


#include "JsonDefine.h"
#include "string_data.h"


using namespace std;

//This is minimum size. If it is decreased , the program will be dead. To use mysql , we need more than 32KBytes.
//PTHREAD_STACK_MIN (16384) bytes. If we set less than PTHREAD_STACK_MIN, default size of stack is set to the created thread.
static const unsigned long 		TREAD_STACK_SIZE=1024*32;



const unsigned char 		SECONDS=60;

//const unsigned char 		SECONDS=3;



void * thread_start(void *arg);


//////////////////////////////////////////////////////////////////////
#ifdef __cplusplus
extern "C" {
#endif

extern void StopNotification(unsigned int id);

#ifdef __cplusplus
}
#endif
//////////////////////////////////////////////////////////////////////


std::string to_string(int value)
{
    std::stringstream strStream;
    strStream<<value;
    return strStream.str();
}




NotifyMessage::NotifyMessage() :mThread_Stack_Size(TREAD_STACK_SIZE),mThreadExit(false),mFirstStarted(true),mPushTitle(PUSH_SERVICE_TITLE)
{

}

NotifyMessage::NotifyMessage(char* packet) 
	: mThread_Stack_Size(TREAD_STACK_SIZE),mThreadExit(false),mFirstStarted(true),mPushTitle(PUSH_SERVICE_TITLE)
{
	log_info("Create NotifyMessage Class");


	//Defualt Value
	mHour =0;
	mMinute =10;
	mInterval =10;
	mNumberOfRepeat =10;
	mNumberMessage=0;
	mIDStartAlarmFromDB =0;

	rapidjson::Document datastring;
	datastring.Parse(packet);

	mUserIndex = datastring[JSON_NAME_USER_INDEX].GetInt(); 


	if( datastring.HasMember(JSON_NAME_ALARM_HOUR) && datastring[JSON_NAME_ALARM_HOUR].IsInt())
		mHour = datastring[JSON_NAME_ALARM_HOUR].GetInt();

	if(  datastring.HasMember(JSON_NAME_ALARM_MINUTE) && datastring[JSON_NAME_ALARM_MINUTE].IsInt())
		mMinute = datastring[JSON_NAME_ALARM_MINUTE].GetInt(); 

	if( datastring.HasMember(JSON_NAME_ALARM_INTERVAL) &&datastring[JSON_NAME_ALARM_INTERVAL].IsInt())
		mInterval = datastring[JSON_NAME_ALARM_INTERVAL].GetInt(); 


	if( datastring.HasMember(JSON_NAME_ALARM_REPEATTIMES) &&datastring[JSON_NAME_ALARM_REPEATTIMES].IsInt() )
		mNumberOfRepeat = datastring[JSON_NAME_ALARM_REPEATTIMES].GetInt(); 


	if( datastring.HasMember(JSON_NAME_ALARM_NUMBER_OF_MESSAGES) &&datastring[JSON_NAME_ALARM_NUMBER_OF_MESSAGES].IsInt()  )
		mNumberMessage = datastring[JSON_NAME_ALARM_NUMBER_OF_MESSAGES].GetInt(); 


	for(int i=0 ; i < mNumberMessage ;i++)
	{
		std::string title =JSON_NAME_ALARM_MESSAGE;
		std::string id= to_string(i);
		title +=id;

		if(datastring.HasMember(title.c_str()))
			mMessages.push_back(datastring[title.c_str()].GetString()); 
		else
			mMessages.push_back("");
		//log_info("Message=%s",mMessages[i].c_str());
	}


	(void) pthread_attr_init(&mConnection_attrib);

	pthread_attr_setstacksize(&mConnection_attrib, mThread_Stack_Size);

	pthread_attr_setscope(&mConnection_attrib, PTHREAD_SCOPE_SYSTEM);

	//When a thread exits , this function returns its resources. Must Set . Or use the other function pthread_detach for it.
	(void) pthread_attr_setdetachstate(&mConnection_attrib, PTHREAD_CREATE_DETACHED);


	int error;


	if (sem_init(&mSemaphpore, 0, 0) == -1)
	{
		log_error("Can't create semophor (error %d)", error);

		//Before go to throw , we delete resource of sem or thread att. Because destructor is not called
		pthread_attr_destroy(&mConnection_attrib);
		throw 0;
		//goto end;
	}

	if ((error = pthread_create(&mThread_Id, &mConnection_attrib, thread_start, (void*)this)))
	{
		// purecov: begin inspected 
		log_error("NotifyMessage :Can't create thread to handle request (error %d)", error);

		//Before go to throw , we delete resource of sem or thread att. Because destructor is not called
		pthread_attr_destroy(&mConnection_attrib);
		sem_destroy(&mSemaphpore);
		throw 0;
		
		//goto end;
		
	}
	
	log_info("NotifyMessage Thread created");

	pthread_attr_destroy(&mConnection_attrib);

end:
	return;

}
	


NotifyMessage::~NotifyMessage()
{
	log_info("Destroy NotifyMessage");

	//Just In case for exiting the thread
	sem_post(&mSemaphpore);
	mThreadExit = true;

	pthread_detach(mThread_Id);

	//pthread_join(mThread_Id,NULL);

	// Not use . The resource of thread is not returned to system.
	//pthread_cancel(mThread_Id);

	sem_destroy(&mSemaphpore);

}


bool NotifyMessage::ExitThread()
{
	
	if( mThreadExit ==true)
	{
		log_info("ExitThread Uid =%d ",mUserIndex);
		//pthread_exit(0);
		return true;
	}
	return false;
}

void NotifyMessage::CleanUp()
{
	//Before Deleting this classs , we need to record the stop time and indexes of message.
	// The problem with sync with the thread , we do it here.
	MysqlDealTask(DB_TASK_STOP_ALARM,0);

	mThreadExit = true;
	log_info("Thread CleanUp");

	//Send the sem to thread for stopping the thread.
	sem_post(&mSemaphpore);

}


void NotifyMessage::WaitTime()
{

	struct timespec ts;

	clock_gettime(CLOCK_REALTIME, &ts);
	log_info("1. ts.tv_sec=%d",ts.tv_sec);	

	if(mFirstStarted ==true)
	{
		mFirstStarted=false;
		ts.tv_sec += mHour*SECONDS*SECONDS + mMinute*SECONDS;
	}
	else
		ts.tv_sec +=mInterval*SECONDS;

	log_info("2. ts.tv_sec=%d",ts.tv_sec);	
	sem_timedwait(&mSemaphpore, &ts);

}


unsigned int NotifyMessage::GetUserID(void)
{
	return mUserIndex; 	
}

unsigned int NotifyMessage::GetPeriodTime(void)
{
	return mInterval; 	
}

unsigned int NotifyMessage::GetNumberRepeat(void)
{
	return mNumberOfRepeat; 	
}

unsigned int NotifyMessage::GetNumberMessages(void)
{
	return mNumberMessage; 	
}

void NotifyMessage::SetTheadIDToDefault(void)
{
	
	mThread_Id =0; 	
}


size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
	char* pmem = NULL;

	int json_size=0;
	int realsize = size * nmemb;
	//std::string response = (char*)contents;
	//char buffer[1024]={0,};

	//memcpy(buffer,contents,realsize);
	return realsize;

}	


void NotifyMessage::PushMessage(int index)
{
	CURL *curl;
	CURLcode res;

	log_info("PushMessage  =%d Index=%d",mMessages.size(),index);
	if( mMessages.size() < index || mMessages.size()==0)
	{
		//log_info("PushMessage  =%d Index=%d",mMessages.size(),index);
		return;
	}
	curl = curl_easy_init();

	struct curl_slist *list = NULL;

	std::string type="android";
	
	std::string url,apikeysend;
	std::string json_data;
	std::string apnsCert,path;

	int device_type=0;

	rapidjson::StringBuffer s;
	rapidjson::Writer<rapidjson::StringBuffer> writer(s);


	if(mDeviceType.compare(type) ==0)
	{
		device_type =0;

		url="https://fcm.googleapis.com/fcm/send";
		const std::string apikey="TestKey";
		apikeysend="Authorization: key=";

		apikeysend =apikeysend +apikey;



		writer.StartObject();	// this means '{'	
		writer.Key("data");

		writer.StartObject();// this means '{'
		writer.Key("title");
		writer.String(mPushTitle.c_str());

		writer.Key("body");
		writer.String(mMessages[index].c_str());
		writer.EndObject(); // this means '}'	

		writer.Key("priority");
		writer.String("high");


		writer.Key("to");
		writer.String(mTokens.c_str());

		writer.EndObject(); // this means '}'
		json_data = s.GetString();
	}
	//iOS Data
	else
	{
		device_type=1;

		//url="https://api.sandbox.push.apple.com:443/3/device/";
		url="https://api.push.apple.com:443/3/device/";

		url=url +mTokens;
	
		apnsCert = "./apns.pem";
	
		writer.StartObject();	// this means '{'	
		writer.Key("aps");

		writer.StartObject();// this means '{'

		writer.Key("mutable-content");
		writer.Uint(1);

		//writer.Key("apns-topic");
		//writer.String("com.timetogo");


		writer.Key("alert");

		writer.StartObject();// this means '{'

		writer.Key("title");
		writer.String(mPushTitle.c_str());
		
		writer.Key("body");
		writer.String(mMessages[index].c_str());
		
		writer.EndObject(); // this means '}'	
	

		writer.Key("sound");
		writer.String("default");

		writer.EndObject(); // this means '}'	
		

		writer.EndObject(); // this means '}'
		json_data = s.GetString();

		//json_data ="{ \"aps\" : { \"mutable-content\":1,\"category\":\"Jake_Ex\", \"alert\" :{ \"title\" :\" Test\", \"body\": \"0123456 \" }, }, }";
		//log_info("json data =%s ",json_data.c_str());


	}


	if(curl) 
	{
		curl_easy_setopt(curl, CURLOPT_POST, 1L); //POST option
		
		//log_info("curl =%s",(char*)curl_version( ));

		//I used utf-8 to Let Hangul not be broken.
		//Append Json Type

		// For Android
		if(device_type ==0)
		{
	
			list = curl_slist_append(list, "Content-Type: application/json ; charset=UTF-8");
			//Append api key for push
			list = curl_slist_append(list, apikeysend.c_str());	
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list); // content-type
		}
		else
		{

			list = curl_slist_append(list, "apns-topic:com.timetogo");
			//Append api key for push
			//list = curl_slist_append(list, path.c_str());	
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list); // content-type
			//curl_easy_setopt(curl, CURLOPT_COOKIE, ";apns-topic = com.timetogo");

			curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);

			curl_easy_setopt(curl, CURLOPT_SSLCERT, apnsCert.c_str()); 
			curl_easy_setopt(curl, CURLOPT_SSLCERTPASSWD, "1234"); 


		}
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str()); 
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);


   		curl_easy_setopt(curl, CURLOPT_POSTFIELDS,json_data.c_str());

		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);

    		/* Perform the request, res will get the return code */
	   	res = curl_easy_perform(curl);
	    	curl_slist_free_all(list);
 
    		/* Check for errors */
    		if(res != CURLE_OK)
      			log_error("curl_easy_perform() failed: %s\n",curl_easy_strerror(res));
 	
    		/* always cleanup */
    		curl_easy_cleanup(curl);	
	}

	return ;
	
}

int NotifyMessage::MysqlDealTask(int task,int index)
{

	MYSQL *con;
	MYSQL_RES *res;
	MYSQL_ROW record;


	int col_count = 0;
	int row_count = 0;

	//Must Use localhost instead 127.0.0.1
	const char *host ="localhost";

	const char *user =MYSQL_USER;
	const char *pw =MYSQL_USER_PW;
	const char *database =MYSQL_DATABASE;
	const char *socket=MYSQL_SOCKET;


	log_info("MysqlDealTask  =%d arg=%d",task,index);

	if ( ExitThread() == true )
		return 1;


	if( (task == DB_TASK_INSERT_USER_MESSAGE) && ( mNumberMessage ==0	|| index >= mNumberMessage) )
		return 1;


	
	con = mysql_init(NULL); 
	if(!con)	
	{
		log_error("Can't Init Mysql ");
		return 0;
	}

	if(!mysql_real_connect(con, host, user, pw, database, 3306, socket, 0))
	{
		log_error("Connect Error %u (%s) : %s \n", mysql_errno(con), mysql_sqlstate(con), mysql_error(con));
		mysql_close(con);
		return 0 ;
	}

	std::string table;
	//MYSQL_RES *res;	
	char query[1024]={0,};
	




	switch(task)
	{
		//Insert data to the table of StartAlarm
		case DB_TASK_START_ALARM:
		{

			struct tm *date; 
			const time_t t = time(NULL); 
			date = localtime(&t);
			
			int tHour =date->tm_hour +mHour;
			int tMin= date->tm_min +mMinute;

			if(tHour >=24)
				tHour =tHour -24;

			if(tMin >=60)
			{
				tMin =tMin -60;
				tHour++;
			}
			table = "StartAlarm";
			//table +="(Id,IDFromUsers,AlarmTime,IntervalValue,RepeatNumber,MessageIndexes,StartTime,EndTime)";
			sprintf(query, "INSERT INTO %s VALUES (0,%d,CONCAT( %d ,':',%d),%d,%d,0,now(),0)",table.c_str(),mUserIndex,tHour,tMin,mInterval,mNumberOfRepeat);

			if ( ExitThread() == true )
				return 1;

			if(mysql_query(con, query))
				log_error("task =%d Error %u (%s) : %s \n", task,mysql_errno(con), mysql_sqlstate(con), mysql_error(con));
			else
			{
				mIDStartAlarmFromDB =mysql_insert_id(con);
			}
			log_info("Insert ID =%d",mIDStartAlarmFromDB);


			//Get Token for Push Message
			memset(query,0x00,sizeof(query));

			sprintf(query, "SELECT DeviceToken,DeviceType FROM Users WHERE UserIndex=%d",mUserIndex);
			if(mysql_query(con, query))
			{
				log_error("task =%d Error %u (%s) : %s \n", task,mysql_errno(con), mysql_sqlstate(con), mysql_error(con));
				return 0 ;

			}	
			else
			{
				res = mysql_store_result(con);
				if(res == NULL)
				{
					log_error(" No result from Query");
					return 0 ;
				}
				else {
					col_count = mysql_num_fields(res);
					//We get only one result 
					if( (record = mysql_fetch_row(res)) != NULL)
					{
						int idx = 0;
						for( idx = 0; idx < col_count ; idx++) {

							if(idx ==0)
								mTokens =record[idx];
							else
								mDeviceType=record[idx];
							
						}

					}
				}
				mysql_free_result(res);


			}
			//log_info("token=%s , Type=%s",mTokens.c_str(),mDeviceType.c_str());
		}
		break;


		case DB_TASK_INSERT_USER_MESSAGE:
		{


		}
		break;


		case DB_TASK_GET_MESSAGES:
		{


		}
		break;


		case DB_TASK_STOP_ALARM:
		{


		}
		break;


	}

	mysql_close(con);
	return 1 ;


}



void * thread_start(void *arg)
{

	NotifyMessage *notiMessage = (NotifyMessage *) arg;

	unsigned int numberrepeat =notiMessage->GetNumberRepeat();
	unsigned int id =notiMessage->GetUserID();

	unsigned int numberMessage =notiMessage->GetNumberMessages();

	int index=0;
	bool justOne=false;

	//Save the variables of Start Alarm into Mysql.
	bool check_ret =notiMessage->MysqlDealTask(NotifyMessage::DB_TASK_START_ALARM,0);

	if(check_ret ==0)
		goto pass;

	for(index ; index <numberrepeat ;index++)
	{
		log_info("!!! NotifyMessage  Thread ID =%ld , Count =%d",id,index+1);

		notiMessage->WaitTime();
		if ( notiMessage->ExitThread() == true )
			break;
		//Get the messages from Public table.
		if(index >=numberMessage && justOne ==false)
		{
			justOne=true;
			check_ret=notiMessage->MysqlDealTask(NotifyMessage::DB_TASK_GET_MESSAGES,index);
			if(check_ret ==0)
				goto pass;
		}
		
		check_ret =notiMessage->MysqlDealTask(NotifyMessage::DB_TASK_INSERT_USER_MESSAGE,index);

		if(check_ret ==0)
			goto pass;

		notiMessage->PushMessage(index);

	}

pass:
	//notiMessage->MysqlDealTask(NotifyMessage::DB_TASK_STOP_ALARM,index);
	
	log_info("*** Noti Thread Out ,  id =%d ,count =%d",id,index+1);
	
	StopNotification(id);

	pthread_exit(0);
}

