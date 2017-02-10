# 安装

```
升级gcc-4.8.x
安装protobuf
编译libhnet.so libhnet.a
```

* 环境最低要求

    * Linux2.6.x（Linux2.4.x + epoll补丁）
    * g++ (GCC) 4.8.x

* 升级gcc-4.8.2

    * 源码安装(CentOS)  #缺少编译环境请先安装老的gcc: yum -y install gcc gcc-c++
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
        * ```此步骤可能会有glibc出错信息。 sudo yum -y install glibc-devel.i686 glibc-devel```
        * make check
        * sudo make install
        * gcc -v #可能要reboot重启生效

* 安装protobuf

    * 源码安装(下载源码https://github.com/google/protobuf)
        * tar -zxvf protobuf-cpp-3.0.0.tar.gz
        * cd protobuf-3.0.0
        * ./configure
        * make
        * make check
        * make install

* 安装hnet

    * 解压hnet.tar.gz到任一目录中，以/usr/local/src为例。
        * cd /usr/local/src
        * tar -zxvf hnet.tar.gz
        * cd hnet/core
        * make
        * make install


[目录](../SUMMARY.md)

[第三章：示例](../example/README.md)
