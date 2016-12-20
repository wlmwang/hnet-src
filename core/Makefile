
###############################
# Copyright (C) Anny Wang.
# Copyright (C) Hupu, Inc.
###############################

CC		:= g++
AR		:= ar
LD		:= ld
ARFLAGS := -rc
LDFLAGS := -shared -fpic -pipe -fno-ident
CFLAGS	:= -Wall -O3 -std=c++11 -D_USE_LOGGER_ -D_DEBUG_

# 第三方库
DIR_LIB		:=	./
LIBFLAGS	:= -L${DIR_LIB} -lpthread -lprotobuf

# 头文件
DIR_CORE	:= ./
INCFLAGS	:= -I${DIR_CORE}

# 源文件
DIR_SRC	:= $(wildcard ${DIR_CORE}/*.cpp ${DIR_CORE}/*.cc)

# 编译文件
DIR_AR	:= ./ar
DIR_LD	:= ./ld
AR_OBJ	:= $(patsubst %.cpp, ${DIR_AR}/%.o, $(notdir ${SRC})) $(patsubst %.cc, ${DIR_AR}/%.o, $(notdir ${SRC}))
LD_OBJ	:= $(patsubst %.cpp, ${DIR_LD}/%.o, $(notdir ${SRC})) $(patsubst %.cc, ${DIR_LD}/%.o, $(notdir ${SRC}))

# 目标文件
LIBNAME	:= libhnet
AR_LIB	:= ${LIBNAME}.a
LD_LIB	:= ${LIBNAME}.so

.PHONY:all clean install

all: $(AR_LIB) $(LD_LIB)

$(AR_LIB):$(AR_OBJ)
	-rm -f $@
	${AR} ${ARFLAGS} $@ $^
	
$(LD_LIB):$(LD_OBJ)
	-rm -f $@
	$(LD) $(LDFLAGS) -o $@ $^
	
$(DIR_AR)/%.o:$(DIR_CORE)/%.cpp
	@echo "Compiling ar $@"
	$(CC) $(CFLAGS) $(INCFLAGS) -c $< -o $@ $(LIBFLAGS)

$(DIR_LD)/%.o:$(DIR_CORE)/%.cpp
	@echo "Compiling ld $@"
	$(CC) $(CFLAGS) $(INCFLAGS) -c $< -o $@ $(LIBFLAGS) $(LDFLAGS)

clean:
	@echo "Clean"
	-rm -rf $(DIR_AR) $(DIR_LD)
	-rm -rf $(AR_LIB) $(LD_LIB)
	
install:
	@echo "Install"
