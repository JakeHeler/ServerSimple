#ifndef __JSON_HEADER_H__
#define __JSON_HEADER_H__

// Basic Define


#define MYSQL_USER		"root"
#define MYSQL_USER_PW	"1234"
#define MYSQL_DATABASE	"ModerateDrink"
#define MYSQL_SOCKET	"/var/lib/mysql/mysql.sock"



/////////////////////////////////////////////////////////////
/* Return Value */
/////////////////////////////////////////////////////////////

// Common
#define	CLIENT_REQUEST_RET_OK	0
#define	CLIENT_REQUEST_RET_NOT_SERVICE_BY_CHECKING_SERVER		1
#define	CLIENT_REQUEST_RET_GOT_WRONG_DATA						2
#define	CLIENT_REQUEST_RET_TOKEN_KEY_DATA_DUPLICATION			3
#define	CLIENT_REQUEST_RET_USER_ID_NOT_EXIST						4
#define	CLIENT_REQUEST_RET_OVER_MAX_STARTALARM_COUNT			5

#define	CLIENT_REQUEST_RET_ALREADY_STARTED						6
#define	CLIENT_REQUEST_RET_NOT_JSON_FORMAT						7
#define	CLIENT_REQUEST_RET_MAX_USER								8


#define	RET_NOT_SERVICE_TIME	10


#define	ERROR_CODE_MYSQL									1000
#define	CLIENT_REQUEST_RET_OPEN_ERROR				(ERROR_CODE_MYSQL +1)
#define	CLIENT_REQUEST_RET_QUERY_ERROR				(ERROR_CODE_MYSQL +2)





//////////////////////////////////////////////////////////////
/* Json Define */ 
//////////////////////////////////////////////////////////////
//Json Cmd


#define	JSON_NAME_RETURN_STATUS				"test"
#define	JSON_NAME_RETURN_STATUS_CONTENTS	"test"

// Sending tokens and smart phone  info
#define	JSON_NAME_TOKENS					"test"
#define 	JSON_NAME_DEVICE_MANUFACTURER 	"test"
#define 	JSON_NAME_DEVICE_MODEL 			"test"
#define 	JSON_NAME_DEVICE_VERSION 			"test"
#define 	JSON_NAME_DEVICE_APP_VERSION 	"test"
#define 	JSON_NAME_DEVICE_TYPE 				"test"

#define 	JSON_NAME_USER_INDEX 				"test"

//Send data of setup
#define 	JSON_NAME_ALARM_HOUR 						"test"
#define 	JSON_NAME_ALARM_MINUTE 					"test"
#define 	JSON_NAME_ALARM_INTERVAL 					"test"
#define 	JSON_NAME_ALARM_REPEATTIMES 				"test"
#define 	JSON_NAME_ALARM_NUMBER_OF_MESSAGES	"test"
#define 	JSON_NAME_ALARM_MESSAGE					"test"




#define	CHECK_SERVER_FILE_NAME			"../check_server.txt"
#define	NOT_SERVICE_TIME_FILE_NAME			"../not_service.txt"


/////////////////////////////////////////////////////////////////////////////////////
#endif

