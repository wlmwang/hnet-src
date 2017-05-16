# 示例

* 安装
    * 服务端：
        * cd /usr/local/hnet/example/server
        * make #修改Makefile编译参数-D_USE_PROTOBUF_，可打开protobuf功能

    * 客户端：
        * cd /usr/local/hnet/example/client
        * make #修改Makefile编译参数-D_USE_PROTOBUF_，可打开protobuf功能

    * 客户端：
        * cd /usr/local/hnet/example/clientd
        * make #修改Makefile编译参数-D_USE_PROTOBUF_，可打开protobuf功能

    * 客户端：
        * cd /usr/local/hnet/example/chttp
        * make #修改Makefile编译参数-D_USE_PROTOBUF_，可打开protobuf功能

* 服务端启动
    * TCP
        * /usr/local/hnet/example/server/examplesvrd -h127.0.0.1 -p10025 -d

    * HTTP
        * /usr/local/hnet/example/server/examplesvrd -h127.0.0.1 -p10025 -d -xhttp

* 客户端启动
    * 单次（TCP）
        * /usr/local/hnet/example/client/exampleclient -h 127.0.0.7 -p 10025

    * 守护（TCP）
        * /usr/local/hnet/example/clientd/exampleclientd -h127.0.0.7 -p 10025

    * 单次（HTTP）
        * /usr/local/hnet/example/chttp/examplehttp -h 127.0.0.7 -p 10025 -xhttp

* 命令
    * 重启
        * /usr/local/hnet/example/server/examplesvrd -s restart

    * 停止
        * /usr/local/hnet/example/server/examplesvrd -s stop


[目录](../SUMMARY.md)

