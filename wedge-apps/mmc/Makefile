############################################################
# source tree layout
############################################################
#SRCDIR ?= ..
#AFTERBURN_SRC = /mmc
AFTERBURN_BUILD ?= $(BUILDDIR)

SRC_BASE = $(AFTERBURN_SRC)/mmc/src
SRC_ACE = $(SRC_BASE)/ace
SRC_BSD = $(SRC_BASE)/bsd
SRC_L4 = $(SRC_BASE)/l4app
SRC_NETWORK = $(SRC_BASE)/network
SRC_SERVICE = $(SRC_BASE)/service
SRC_STREAMING = $(SRC_BASE)/streaming
SRC_THREAD = $(SRC_BASE)/thread
SRC_TEST = $(SRC_BASE)/test

INCLUDE_BASE = $(AFTERBURN_SRC)/mmc/include
INCLUDE_ACE = $(INCLUDE_BASE)/ace
INCLUDE_BSD = $(INCLUDE_BASE)/bsd
INCLUDE_L4 = $(INCLUDE_BASE)/l4app
INCLUDE_NETWORK = $(INCLUDE_BASE)/network
INCLUDE_SERVICE = $(INCLUDE_BASE)/service
INCLUDE_STREAMING = $(INCLUDE_BASE)/streaming
INCLUDE_THREAD = $(INCLUDE_BASE)/thread
INCLUDE_UTIL = $(INCLUDE_BASE)/util

BUILD_BASE = $(AFTERBURN_BUILD)
BUILD_ACE = $(BUILD_BASE)
BUILD_BSD = $(BUILD_BSD)
BUILD_L4 = $(BUILD_BASE)
BUILD_NETWORK = $(BUILD_BASE)
BUILD_SERVICE = $(BUILD_BASE)
BUILD_THREAD = $(BUILD_BASE)
BUILD_TEST = $(BUILD_BASE)
BUILD_UTIL = $(BUILD_BASE)

############################################################
# Network
############################################################
SOURCES = $(SRC_NETWORK)/Acceptor.cpp
SOURCES += $(SRC_NETWORK)/ServerSocket.cpp
SOURCES += $(SRC_NETWORK)/Connector.cpp
HEADERS = $(INCLUDE_NETWORK)/Acceptor.h
HEADERS += $(INCLUDE_NETWORK)/ServerSocket.h
HEADERS += $(INCLUDE_NETWORK)/Socket.h
HEADERS += $(INCLUDE_NETWORK)/NetworkException.h
HEADERS += $(INCLUDE_NETWORK)/Connector.h
OBJS = $(SRC_NETWORK)/Acceptor.o
OBJS += $(SRC_NETWORK)/ServerSocket.o
OBJS += $(SRC_NETWORK)/Connector.o

############################################################
# Service
############################################################
SOURCES += $(SRC_SERVICE)/ServiceManager.cpp
SOURCES += $(SRC_SERVICE)/MigrationService.cpp
SOURCES += $(SRC_SERVICE)/Request.cpp
HEADERS += $(INCLUDE_SERVICE)/Service.h
HEADERS += $(INCLUDE_SERVICE)/ServiceManager.h
HEADERS += $(INCLUDE_SERVICE)/MigrationService.h
HEADERS += $(INCLUDE_SERVICE)/Request.h
OBJS += $(SRC_SERVICE)/ServiceManager.o
OBJS += $(SRC_SERVICE)/MigrationService.o
OBJS += $(SRC_SERVICE)/Request.o

############################################################
# Thread
############################################################
#SOURCES += ThreadManager.cpp BoostThread.cpp
#HEADERS += ThreadManager.h BoostThread.hpp ThreadBase.h Runnable.h
#OBJS += ThreadManager.o BoostThread.o

############################################################
# Utilities
############################################################
HEADERS += $(INCLUDE_UTIL)/NonCopyable.h

############################################################
# Streaming
############################################################
SOURCES += $(SRC_STREAMING)/InputStream.cpp
HEADERS += $(INCLUDE_STREAMING)/InputStream.h
OBJS += $(SRC_STREAMING)/InputStream.o
SOURCES += $(SRC_STREAMING)/OutputStream.cpp
HEADERS += $(INCLUDE_STREAMING)/OutputStream.h
OBJS += $(SRC_STREAMING)/OutputStream.o
HEADERS += $(INCLUDE_STREAMING)/StreamingException.h
HEADERS += $(INCLUDE_STREAMING)/Stream.h

############################################################
# ACE Mocks
############################################################
#SOURCES += $(SRC_ACE)/SOCK_Acceptor.cpp
#SOURCES += $(SRC_ACE)/SOCK_Stream.cpp
#SOURCES += $(SRC_ACE)/SOCK_Connector.cpp
#HEADERS += $(INCLUDE_ACE)/SOCK_Acceptor.h
#HEADERS += $(INCLUDE_ACE)/SOCK_Stream.h
#HEADERS += $(INCLUDE_ACE)/SOCK_Connector.h
#HEADERS += $(INCLUDE_ACE)/INET_Addr.h
#OBJS += $(SRC_ACE)/SOCK_Acceptor.o
#OBJS += $(SRC_ACE)/SOCK_Stream.o
#OBJS += $(SRC_ACE)/SOCK_Connector.o

############################################################
# BSD Sockets
############################################################
SOURCES += $(SRC_BSD)/SOCK_Acceptor.cpp
SOURCES += $(SRC_BSD)/SOCK_Stream.cpp
SOURCES += $(SRC_BSD)/SOCK_Connector.cpp
HEADERS += $(INCLUDE_BSD)/SOCK_Acceptor.h
HEADERS += $(INCLUDE_BSD)/SOCK_Stream.h
HEADERS += $(INCLUDE_BSD)/SOCK_Connector.h
OBJS += $(SRC_BSD)/SOCK_Acceptor.o
OBJS += $(SRC_BSD)/SOCK_Stream.o
OBJS += $(SRC_BSD)/SOCK_Connector.o

############################################################
# L4 Adapter
############################################################
SOURCES += $(SRC_L4)/Resourcemon.cpp
#SOURCES += $(SRC_L4)/ResourcemonMock.cpp
HEADERS += $(INCLUDE_L4)/Resourcemon.h
OBJS += $(SRC_L4)/Resourcemon.o
#OBJS += $(SRC_L4)/ResourcemonMock.o

############################################################
# Apps
############################################################
#SOURCES += src/ctrl/mmcCtrl.cpp
#OBJS += src/ctrl/mmcCtrl.o

SOURCES += src/mmcd/mmcd.cpp
OBJS += src/mmcd/mmcd.o

############################################################
# L4 Specifica
############################################################
INCLUDE  += -I $(L4KA_PISTACHIO_USER)/include -I$(L4KA_INTERFACE_DIR) -I $(AFTERBURN_BUILD)

CXXFLAGS += $(shell $(IDL4_CONFIG) --cflags)

LIBS += -ll4

IDL4 ?= idl4
IDL4_CONFIG ?= idl4-config

############################################################
# Tests
############################################################
#SOURCES += $(SRC_TEST)/AcceptorTest.cpp
#OBJS += $(SRC_TEST)/AcceptorTest.o
# test targets
TESTS = AcceptorTest

DELETE = rm
LIBINCLUDE = -L$(L4KA_PISTACHIO_USER)/lib/l4 $(COMMON_BUILD) $(AFTERBURN_BUILD)/mmcd.o $(LIBS)
INCLUDE += -Iinclude
#CXX = c:/mingw/bin/c++
CXX = /usr/bin/g++
CPPFLAGS += -Wall $(shell $(IDL4_CONFIG) --cflags) -O2


#LIBS = -lwsock32

# network
COMMON_BUILD = $(AFTERBURN_BUILD)/Acceptor.o $(AFTERBURN_BUILD)/Connector.o $(AFTERBURN_BUILD)/ServerSocket.o
# streams
COMMON_BUILD += $(AFTERBURN_BUILD)/OutputStream.o $(AFTERBURN_BUILD)/InputStream.o
# servive
COMMON_BUILD += $(AFTERBURN_BUILD)/MigrationService.o $(AFTERBURN_BUILD)/Request.o $(AFTERBURN_BUILD)/ServiceManager.o
# l4
COMMON_BUILD += $(AFTERBURN_BUILD)/Resourcemon.o
# bsd
COMMON_BUILD += $(AFTERBURN_BUILD)/SOCK_Acceptor.o $(AFTERBURN_BUILD)/SOCK_Connector.o $(AFTERBURN_BUILD)/SOCK_Stream.o

.PHONY : clean all
all: $(OBJS)
	echo $(OBJS)
	$(CXX) -o mmcd $(LIBINCLUDE) $(LIBS)

mmcd:
	$(CXX) $(LIBINCLUDE) -o mmcd 

$(OBJS) : $(SOURCES) $(HEADERS)  $(IDL_SERVER_OUTPUT) $(IDL_CLIENT_OUTPUT)
	$(CXX) $(CPPFLAGS) $(INCLUDE) -c $*.cpp -o $(AFTERBURN_BUILD)/$(@F)


clean:
	$(DELETE) -f $(AFTERBURN_BUILD)/*.o
	$(DELETE) -f $(TESTS)
