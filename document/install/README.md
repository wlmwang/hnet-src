# 安装


```
安装examplesvrd、exampleclient、exampledaemon
目标：查看Makefile，熟悉该框架依赖文件。
```

* 环境最低要求

    * Linux2.6.x（Linux2.4.x + epoll补丁）

    * g++ (GCC) 4.8.x

* make && make install

    * 解压hnet.tar.gz到系统目录中，以 /usr/local/hnet 为例。
        * cd /usr/local
        * tar -zxvf hnet.tar.gz
        * cd hnet/example

    * 安装examplesvrd
        * make

    * 安装exampleclient
        * make -f MakefileClient

    * 安装exampledaemon
        * make -f MakefileDaemon



[示例](example/README.md)
