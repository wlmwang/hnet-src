
###############################
# Copyright (C) Anny Wang.
# Copyright (C) Hupu, Inc.
###############################

#
# 编译环境要求
# GCC 4.8+ #调试要求gdb7.6+
# Linux2.6+ 或 Linux2.4+附epoll补丁
# 
# 运行要求
# GCC 4.4+
# Linux2.6+ 或 Linux2.4+附epoll补丁
# 如需protobuf组件，需预先编译vendor目录下的protobuf软件包，并打开 -D_USE_PROTOBUF_ 及 -lprotobuf 编译参数
#

CC		:= g++
AR		:= ar
LD		:= g++
ARFLAGS := -fpic -pipe -fno-ident  #便于其他***.so库 静态链接 ${LIBNAME}.a 库
LDFLAGS := -fpic -pipe -fno-ident
CFLAGS	:= -Wall -O3 -std=c++11 #-D_USE_PROTOBUF_ -D_USE_LOGGER_ -D_DEBUG_

# 第三方库
ARLIBFLAGS	:=
LDLIBFLAGS	:= -L/usr/local/lib -lpthread #-lprotobuf

# 主目录
DIR_SRC		:= .
DIR_MSG		:= .

# 头文件
INCFLAGS	:= -I${DIR_SRC}

# 源文件
CPP_SRC	:= $(wildcard ${DIR_SRC}/*.cpp)
CC_SRC	:= $(wildcard ${DIR_MSG}/*.cc)

# 编译文件
DIR_AR	:= ./ar
DIR_LD	:= ./ld
AR_OBJ	:= $(patsubst %.cpp, ${DIR_AR}/%.o, $(notdir ${CPP_SRC})) $(patsubst %.cc, ${DIR_AR}/%.o, $(notdir ${CC_SRC}))
LD_OBJ	:= $(patsubst %.cpp, ${DIR_LD}/%.o, $(notdir ${CPP_SRC})) $(patsubst %.cc, ${DIR_LD}/%.o, $(notdir ${CC_SRC}))

# 目标文件
LIBNAME	:= libhnet
AR_LIB	:= ${LIBNAME}.a
LN_LIB	:= ${LIBNAME}.so
LD_LIB	:= ${LIBNAME}.so.0.0.18
SN_LIB	:= ${LIBNAME}.so.0

# 安装目录
INS_LIB	:= /usr/local/lib
INS_INC	:= /usr/local/include/hnet

.PHONY:all clean install

all: ${AR_LIB} ${LD_LIB}

${AR_LIB}:${AR_OBJ}
	-rm -f $@
	${AR} -rcs $@ $^
	
${LD_LIB}:$(LD_OBJ)
	-rm -f $@
	${LD} -shared -Wl,-soname,${SN_LIB} -o $@ $^ ${CFLAGS} ${LDLIBFLAGS}

# 编译ar
${DIR_AR}/%.o:${DIR_SRC}/%.cpp
	@echo "Compiling $@"
	${CC} ${CFLAGS} ${INCFLAGS} -c $< -o $@ ${ARFLAGS}

${DIR_AR}/%.o:${DIR_SRC}/%.cc
	@echo "Compiling $@"
	${CC} ${CFLAGS} ${INCFLAGS} -c $< -o $@ ${ARFLAGS}

# 编译ld
${DIR_LD}/%.o:${DIR_SRC}/%.cpp
	@echo "Compiling $@"
	${CC} ${CFLAGS} ${INCFLAGS} -c $< -o $@ ${LDFLAGS}

${DIR_LD}/%.o:${DIR_SRC}/%.cc
	@echo "Compiling $@"
	${CC} ${CFLAGS} ${INCFLAGS} -c $< -o $@ ${LDFLAGS}

clean:
	@echo "Clean and Rebuild"
	-rm -rf ${DIR_AR} ${DIR_LD} ${AR_LIB} ${LN_LIB} ${LD_LIB} ${SN_LIB}
	-mkdir ${DIR_AR} ${DIR_LD}
	
install:
	@echo "Install LIB and HEADER"
	-rm -rf ${INS_INC}
	-mkdir ${INS_INC}
	-cp -f ${DIR_SRC}/*.h ${INS_INC}
	-cp -f ${LIBNAME}.*	${INS_LIB}
	-ldconfig
	-ln -fs ${LD_LIB} ${INS_LIB}/${LN_LIB}
