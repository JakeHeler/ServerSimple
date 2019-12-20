#ifndef __JSON_HEADER_H__
#define __JSON_HEADER_H__

// Basic Define


// http url
//#define FOMAL_IP_SERVER_DEFINED



#define	NO_RESPONSE_SERVER	(-1)
///////////////////////////////////////////////////////////////////////////////////////////////

//****************************************//
// These defines must be the same as jsonindex.php and ret.php
//****************************************//


/////////////////////////////////////////////////////////////
/* Return Value */
/////////////////////////////////////////////////////////////

// Common
#define	HTTP_REQUEST_RET_OK									0
#define	HTTP_REQUEST_RET_NOT_SERVICE_BY_CHECKING_SERVER		1
#define	HTTP_REQUEST_RET_GOT_WRONG_DATA						2
#define	HTTP_REQUEST_RET_KEY_DATA_DUPLICATION				3
#define	HTTP_REQUEST_RET_USER_ID_NOT_EXIST					4
#define	HTTP_REQUEST_RET_OVER_MAX_STARTALARM_COUNT			5

#define	HTTP_REQUEST_RET_COMMUNICATION_ERROR_WITH_SERVER	6




#define	HTTP_REQUEST_RET_SOCKET_OPEN_ERROR		100
#define	HTTP_REQUEST_RET_SOCKET_CONNECT_ERROR	101

#define	HTTP_REQUEST_RET_MYSQL_OPEN_ERROR	200
#define	HTTP_REQUEST_RET_SOCKET_QUERY_ERROR	201



//////////////////////////////////////////////////////////////
/* Json Define */ 
//////////////////////////////////////////////////////////////
//Json Cmd



#define	JSON_NAME_RETURN_STATUS			"test"

// Sending tokens and smart phone  info
#define	JSON_NAME_TOKENS					"test"
#define 	JSON_NAME_DEVICE_MANUFACTURER 	"test"
#define 	JSON_NAME_DEVICE_MODEL 			"test"
#define 	JSON_NAME_DEVICE_VERSION 			"test"
#define 	JSON_NAME_DEVICE_APP_VERSION 		"test"
#define 	JSON_NAME_DEVICE_TYPE 				"test"

#define 	JSON_NAME_USER_INDEX 				"test"

//Send data of setup
#define 	JSON_NAME_ALARM_HOUR 						"test"
#define 	JSON_NAME_ALARM_MINUTE 					"test"
#define 	JSON_NAME_ALARM_INTERVAL 					"test"
#define 	JSON_NAME_ALARM_REPEATTIMES 				"test"
#define 	JSON_NAME_ALARM_NUMBER_OF_MESSAGES		"test"
#define 	JSON_NAME_ALARM_MESSAGE					"test"

//Receive Push Message
#define 	JSON_NAME_PUSH_NUMBER_MESSAGE			"test"
#define 	JSON_NAME_PUSH_MESSAGE					"test"
#define 	JSON_NAME_PUSH_MESSAGE_RECIEVED_TIME		"test"


#define 	JSON_NAME_THIS_SCENE_FOR_PUSHMESSAGE	"test"


/////////////////////////////////////////////////////////////////////////////////////
#endif

