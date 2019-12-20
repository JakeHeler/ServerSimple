#include <mysql.h>

#include "my_global.h"

#include <semaphore.h>
#include <vector>
#include <string>


//typedef void  *(*pthread_handler)(void*);


class NotifyMessage
{
public:

	enum db_task{
		DB_TASK_START_ALARM=0,
		DB_TASK_INSERT_USER_MESSAGE=1,	
		DB_TASK_GET_MESSAGES,	
		DB_TASK_STOP_ALARM,

	}DB_Task_Command;

public:
	NotifyMessage() ;

	virtual ~NotifyMessage();

	NotifyMessage(char* packet) ;


	unsigned int GetUserID(void);
	void WaitTime();
	void CleanUp();
	bool ExitThread();
	unsigned int GetPeriodTime(void);
	unsigned int GetNumberRepeat(void);
	void SetTheadIDToDefault(void);
	void PushMessage(int index);
	int MysqlDealTask(int task,int arg);

	unsigned int GetNumberMessages(void);


private:
	//pthread_handler HandleConnection;
	const std::string 	mPushTitle;


	const unsigned long 		mThread_Stack_Size;	
	unsigned int		mUserIndex ,mHour ,mMinute,mInterval, mNumberOfRepeat,mNumberMessage;
	std::vector <std::string> mMessages;
	pthread_attr_t 		mConnection_attrib;
	pthread_t  mThread_Id;
	sem_t mSemaphpore;
	bool		mThreadExit;
	bool		mFirstStarted;
	unsigned long mIDStartAlarmFromDB;
	std::vector <unsigned long> mIDInsertMessage;
	std::string 	mTokens;
	std::string 	mDeviceType;
};



