# Copyright (c)
# Process this file with automake to create Makefile.in

CFLAGS = -Wall -Wextra -Wunused -Wwrite-strings -Wno-strict-aliasing -Wdeclaration-after-statement

AR = ar
ARFLAGS = cru
AS = gcc -c
ASFLAGS = 


CC = /usr/bin/gcc

# this version is 4.8
#CC = /opt/rh/devtoolset-2/root/usr/bin/gcc

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

CXX = /usr/bin/g++

# this version is 4.8
#CXX = /opt/rh/devtoolset-2/root/usr/bin/g++

CXXCPP = g++ -E
CXXDEPMODE = depmode=gcc3
#CXXFLAGS = -g -DSAFE_MUTEX -DSAFEMALLOC -fno-implicit-templates -fno-exceptions -fno-rtti
CXXLDFLAGS = 
CXX_VERSION = g++ (GCC) 4.1.2 20080704 (Red Hat 4.1.2-55)

#Include Thread Lib
THREAD_LIBS = -lpthread -lcurl

DEFAULT_INCLUDES = -I.

###################
#Debug Option
###################
DEBUGOPTION= -DDEBUG_OPNTION=1


GETTIMELIB= -lrt

####################
# Include
####################

JSON_DIRECTORY=./json/

INCLUDES = -I$(JSON_DIRECTORY)
		
GDB_ENABLE= -g

###################
#Taget Name
###################
TARGET=ClientTest

#Compile
COMPILE = $(CC) $(DEFAULT_INCLUDES) $(INCLUDES) $(CPPFLAGS)$(CFLAGS) $(DEBUGOPTION)  $(GDB_ENABLE)

CXXCOMPILE = $(CXX) $(DEFAULT_INCLUDES) $(INCLUDES) $(CPPFLAGS) $(DEBUGOPTION)  $(GDB_ENABLE)
#################################
#make binary file for executing
#################################

SRCS = log.c curve25519-donna-c64.c aes.c
OBJS = $(SRCS:.c=.o)

.c.o:
	$(COMPILE) -c $<

##############
# Compile for C++
##############
CPPSRCS =clientTest.cc
CPPOBJS = $(CPPSRCS:.cc=.o)

.cc.o:
	$(CXXCOMPILE) -c $<



##################	
#all: main
##################
all : $(OBJS) $(CPPOBJS)
	@echo "#################################"
	@echo "Building Executing Image"
	$(CXXCOMPILE) -o $(TARGET) $(OBJS) $(CPPOBJS) $(THREAD_LIBS) $(GETTIMELIB)
	@echo "Building Done"	
	@echo "#################################"

#################################
## clean
#################################
clean:
	rm -f *.o $(TARGET)
##################################

