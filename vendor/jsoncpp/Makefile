
###############################
# Copyright (C) Anny Wang.
# Copyright (C) Hupu, Inc.
###############################

#
# jsoncpp头文件: jsoncpp-1.8.0/include/json
# jsoncpp源文件: jsoncpp-1.8.0/src/lib_json
#

CC		:= g++
AR		:= ar
LD		:= g++
ARFLAGS := -rc
LDFLAGS := -fpic -pipe -fno-ident
CFLAGS	:= -Wall -O3 -Wno-unknown-pragmas -Wno-format

# 第三方库
DIR_LIB		:=
LIBFLAGS	:=

# 主目录
DIR_SRC		:= .
INC_SRC		:= ./include

# 头文件
INCFLAGS	:= -I${DIR_SRC} -I${INC_SRC}

# 源文件
CPP_SRC	:= $(wildcard ${DIR_SRC}/*.cpp)

# 编译文件
DIR_AR	:= ./ar
DIR_LD	:= ./ld
AR_OBJ	:= $(patsubst %.cpp, ${DIR_AR}/%.o, $(notdir ${CPP_SRC}))
LD_OBJ	:= $(patsubst %.cpp, ${DIR_LD}/%.o, $(notdir ${CPP_SRC}))

# 目标文件
LIBNAME	:= libjsoncpp
AR_LIB	:= ${LIBNAME}.a
LD_LIB	:= ${LIBNAME}.so

# 安装目录
INS_LIB	:= /usr/local/lib
INS_INC	:= /usr/local/include/jsoncpp

.PHONY:all clean install

all: ${AR_LIB} ${LD_LIB}

${AR_LIB}:${AR_OBJ}
	-rm -f $@
	${AR} ${ARFLAGS} $@ $^
	
${LD_LIB}:${LD_OBJ}
	-rm -f $@
	${LD} -shared -o $@ $^ ${CFLAGS} ${LIBFLAGS}

# 编译ar
${DIR_AR}/%.o:${DIR_SRC}/%.cpp
	@echo "Compiling $@"
	${CC} ${CFLAGS} ${INCFLAGS} -c $< -o $@

# 编译ld
${DIR_LD}/%.o:${DIR_SRC}/%.cpp
	@echo "Compiling $@"
	${CC} ${CFLAGS} ${INCFLAGS} -c $< -o $@ ${LDFLAGS}

clean:
	@echo "Clean and Rebuild"
	-rm -rf ${DIR_AR} ${DIR_LD} ${AR_LIB} ${LD_LIB}
	-mkdir ${DIR_AR} ${DIR_LD}

install:
	@echo "Install LIB and HEADER"
	-rm -rf ${INS_INC}
	-mkdir ${INS_INC}
	-cp -rf ${INC_SRC}/json ${INS_INC}
	-cp -f ${LIBNAME}.* ${INS_LIB}
