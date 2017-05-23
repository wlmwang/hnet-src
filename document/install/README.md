# 安装

```
升级gcc-4.8+（编译依赖）
安装protobuf（必要时安装protobuf）
安装tinyxml（必要时安装tinyxml）
安装hnet（目前几个项目多数使用静态库，并将其编译为可静态链接到到动态库版本）
```

* 环境最低要求

    * Linux2.6+（Linux2.4+ + epoll补丁）
    * gcc-4.8+（运行时可为gcc-4.4+）

* 升级gcc-4.8.2

    * 源码安装(CentOS)  #其他系统请自行升级。缺少编译环境请先安装老的gcc: yum -y install gcc gcc-c++
        * cd /usr/local/src
        * wget http://ftp.gnu.org/gnu/gcc/gcc-4.8.2/gcc-4.8.2.tar.bz2
        * tar -jxvf gcc-4.8.2.tar.bz2
        * cd gcc-4.8.2
        * ./contrib/download_prerequisites  #下载供编译需求依赖项
        * cd ..
        * mkdir gcc-build-4.8.2     #建立一个目录供编译出的文件存放
        * cd gcc-build-4.8.2
        * ../gcc-4.8.2/configure --enable-checking=release --enable-languages=c,c++ --disable-multilib      #生成Makefile文件
        * make -j4      #极其耗时。-j4选项是make对多核处理器的优化
        * ```此步骤可能会有glibc出错信息。 yum -y install glibc-devel.i686 glibc-devel```

        * make check
        * make install
        * gcc -v #可能要reboot重启生效
		* cp stage1-x86_64-unknown-linux-gnu/libstdc++-v3/src/.libs/libstdc++.so.6.0.18 /usr/lib64 	#替换libstdc++运行库
		* ldconfig
		* strings /usr/lib64/libstdc++.so.6 | grep GLIBC

* 安装protobuf（默认路径 /usr/local/lib   /usr/local/include/google）

    * 源码安装(hnet/vendor目录附带有2.4.1版本。官方https://github.com/google/protobuf)
        * tar -zxvf protobuf-2.4.1.tar.gz
        * cd protobuf-2.4.1
        * ./configure
        * make
        * make check
        * make install
        * echo "/usr/local/lib" > /etc/ld.so.conf.d/protobuf.conf #如已在搜索路径无需此步
        * ldconfig

* 安装tinyxml（默认路径  /usr/local/lib   /usr/local/include/tinyxml）
    * 源码安装（hnet/vendor目录附带有源码）
        * make
        * make install
        * echo "/usr/local/lib" > /etc/ld.so.conf.d/tinyxml.conf #如已在搜索路径无需此步
        * ldconfig

* 安装hnet

    * 解压hnet.tar.gz到任一目录中，以/usr/local/src为例
        * cd /usr/local/src
        * tar -zxvf hnet.tar.gz
        * cd hnet/core
        * make  #修改Makefile编译参数-D_USE_PROTOBUF_，可打开protobuf功能
        * ```此步骤可能会有protobuf出错信息。请使用本机安装的protoc重新编译wChannel.proto文件   protoc -I./ --cpp_out=./ ./wChannel.proto```

        * make install
        * echo "/usr/local/lib" > /etc/ld.so.conf.d/hnet.conf #如已在搜索路径无需此步
        * ldconfig


[目录](../SUMMARY.md)
