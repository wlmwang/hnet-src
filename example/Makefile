
###############################
# Copyright (C) Anny Wang.
# Copyright (C) Hupu, Inc.
###############################

#
# Makefile
# 动态链接所需库，需.so文件安装到ldconfig加载路径中(/usr/local/lib)
# 需要预先编译vendor目录下的protobuf软件包
#

CXX=g++ -std=c++11
CXXFLAGS=-O3 -Wall -D_USE_LOGGER_ -D_DEBUG_
LDFLAGS=-L/usr/local/lib -lpthread -lprotobuf

COREPATH=../core
COMMPATH=../command
MSGPATH=../message
VENDOR=../vendor
INCPATH=-I./$(COREPATH) -I./$(COMMPATH) -I./$(MSGPATH)

SRCPATH=.
OBJECTS=./$(COREPATH)/wCrc32c.o \
		./$(COREPATH)/wEnv.o \
		./$(COREPATH)/wFile.o \
		./$(COREPATH)/wLogger.o \
		./$(COREPATH)/wMisc.o \
		./$(COREPATH)/wMemPool.o \
		./$(COREPATH)/wPing.o \
		./$(COREPATH)/wStatus.o \
		./$(COREPATH)/wProcTitle.o \
		./$(COREPATH)/wSignal.o \
		./$(COREPATH)/wMemPool.o \
		./$(COREPATH)/wShm.o \
		./$(COREPATH)/wThread.o \
		./$(COREPATH)/wSocket.o \
		./$(COREPATH)/wChannelSocket.o \
		./$(COREPATH)/wTcpSocket.o \
		./$(COREPATH)/wUnixSocket.o \
		./$(COREPATH)/wTask.o \
		./$(COREPATH)/wChannelTask.o \
		./$(COREPATH)/wSingleClient.o \
		./$(COREPATH)/wMultiClient.o \
		./$(COREPATH)/wMaster.o \
		./$(COREPATH)/wWorker.o \
		./$(COREPATH)/wConfig.o \
		./$(COREPATH)/wServer.o \
		./$(COREPATH)/wChannel.pb.o \
		\
		./$(MSGPATH)/example.pb.o \
		./$(SRCPATH)/exampleServer.o

TARGET=examplesvrd

.PHONY:all clean install

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

%.o:%.cpp
	$(CXX) $(CXXFLAGS) $(INCPATH) -c $< -o $@

%.d:%.cpp
	@set -e; rm -f $@; $(CXX) -MM $< $(INCPATH) > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

-include $(OBJECTS:.o=.d)

clean:
	-rm -f $(COREPATH)/*.o $(COREPATH)/*.d $(COREPATH)/*.d.*
	-rm -f $(TARGET) $(SRCPATH)/*.o $(SRCPATH)/*.d $(SRCPATH)/*.d.*