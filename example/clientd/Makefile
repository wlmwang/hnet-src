
###############################
# Copyright (C) Anny Wang.
# Copyright (C) Hupu, Inc.
###############################

#
# gcc 4.8+(gdb7.6+)
# 需要预先编译vendor目录下的protobuf软件包；core下hnet
# 所需的.so文件建议安装到ldconfig加载路径中(/usr/local/lib)
#

CC		:= g++
CFLAGS	:= -Wall -O3 -std=c++11 -D_DEBUG_ -D_USE_LOGGER_ #-D_USE_PROTOBUF_
ARFLAGS	:= -Wl,-dn #-Wl,-Bstatic
LDFLAGS	:= -Wl,-dy #-Wl,-Bdynamic

# 第三方库
DIR_INC		:= -I/usr/local/include/hnet
DIR_LIB		:= -L/usr/local/lib
LIBFLAGS	:= ${DIR_LIB} $(ARFLAGS) -lhnet -lprotobuf $(LDFLAGS) -lpthread

# 主目录,message,command目录
DIR_SRC		:= ./
DIR_MSG		:= ../../message/
DIR_CMD		:= ../../command/

# 头文件
INCFLAGS	:= ${DIR_INC} -I${DIR_SRC} -I${DIR_MSG} -I${DIR_CMD}

# 源文件
CPP_SRC	:= $(wildcard ${DIR_SRC}*.cpp)
CC_SRC	:= $(wildcard ${DIR_MSG}*.cc)

# 编译文件
OBJ		:= $(patsubst %.cpp, %.o, $(notdir ${CPP_SRC})) $(patsubst %.cc, %.o, $(notdir ${CC_SRC}))

TARGET	:= exampleclientd

.PHONY:all clean install

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LIBFLAGS)

%.o:${DIR_SRC}%.cpp
	$(CC) $(CFLAGS) $(INCFLAGS) -c $< -o $@

%.o:${DIR_MSG}%.cc
	$(CC) $(CCFLAGS) $(INCFLAGS) -c $< -o $@

clean:
	-rm -f $(TARGET) $(DIR_SRC)*.o $(DIR_MSG)*.o
