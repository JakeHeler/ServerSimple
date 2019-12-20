# Copyright (c)
# Process this file with automake to create Makefile.in

CFLAGS = -Wall -Wextra -Wunused -Wwrite-strings -Wno-strict-aliasing -Wdeclaration-after-statement

AR = ar
ARFLAGS = cru
AS = gcc -c
ASFLAGS = 


CC = /usr/bin/gcc
CCAS = gcc
CCASDEPMODE = depmode=gcc3
CCASFLAGS = -g -O2    -Wa,--noexecstack
CC_VERSION = gcc version 4.8.2 20140120 (Red Hat 4.8.2-15) (GCC)

CHMOD = /bin/chmod
CLIENT_EXTRA_LDFLAGS = 
CLIENT_THREAD_LIBS = -lpthread

CP = /bin/cp
CPP = gcc -E
CPPFLAGS = -Wall -fexceptions -fno-rtti

#########
#C++ gcc
#########
CXX = /usr/bin/g++
CXXCPP = g++ -E
CXXDEPMODE = depmode=gcc3
CXXFLAGS = -g -DSAFE_MUTEX -DSAFEMALLOC -fno-implicit-templates -fno-exceptions -fno-rtti
CXXLDFLAGS = 
CXX_VERSION = g++ (GCC) 4.1.2 20080704 (Red Hat 4.1.2-55)

#Include Thread Lib
THREAD_LIBS = -lpthread

DEFAULT_INCLUDES = -I.

###################
#Debug Option
###################
DEBUGOPTION= -DDEBUG_OPNTION=1


GETTIMELIB= -lrt

DYNAICLOAD=-ldl
####################
# Include
####################

JSON_DIRECTORY=./json/
MYSQL_INC =-I/usr/local/mysql/include/mysql

INCLUDES = -I$(JSON_DIRECTORY) $(MYSQL_INC)
		
GDB_ENABLE= -g

###################
#Mysql Lib
###################
#LIBS    =-lcurl -rdynamic -L/usr/lib64/mysql -lmysqlclient -lz -lcrypt -lnsl -lm

LIBS    = -L/usr/lib64/mysql -lmysqlclient  -L/usr/lib64 -lcurl

MYSQLCFLAGS =-pipe -Wp,-fexceptions -fstack-protector --param=ssp-buffer-size=4 -m64 -fwrapv


###################
#Taget Name
###################

TARGET=ServerRun

#Compile
COMPILE = $(CC) $(DEFAULT_INCLUDES) $(INCLUDES) $(CFLAGS) $(DEBUGOPTION)  $(GDB_ENABLE)

CXXCOMPILE = $(CXX) $(DEFAULT_INCLUDES) $(INCLUDES) $(CPPFLAGS) $(DEBUGOPTION)  $(GDB_ENABLE)
#################################
#make binary file for executing
#################################

SRCS = log.c viosocket.c my_getsystime.c aes.c curve25519-donna-c64.c
OBJS = $(SRCS:.c=.o)

.c.o:
	$(COMPILE) -c $<

##############
# Compile for C++
##############
CPPSRCS =server.cc vio.cc net_serv.cc net_connect.cc NotifyMessage.cc
CPPOBJS = $(CPPSRCS:.cc=.o)

.cc.o:
	$(CXXCOMPILE) -c $<



##################	
#all: main
##################
all : $(OBJS) $(CPPOBJS)
	@echo "#################################"
	@echo "Building Executing Image"
	$(CXXCOMPILE) -o $(TARGET) $(OBJS) $(CPPOBJS) $(THREAD_LIBS) $(GETTIMELIB) $(LIB) $(LIBS) $(DYNAICLOAD)
	@echo "Building Done"	
	@echo "#################################"

#################################
## clean
#################################
clean:
	rm -f *.o $(TARGET)
##################################

