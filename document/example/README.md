# 示例

* 安装
    * 服务端：（修改Makefile -D_USE_PROTOBUF_ 可打开protobuf功能）
        * cd /usr/local/hnet/example/server
        * make

    * 客户端：（修改Makefile -D_USE_PROTOBUF_ 可打开protobuf功能）
        * cd /usr/local/hnet/example/client
        * make

    * 客户端：（修改Makefile -D_USE_PROTOBUF_ 可打开protobuf功能）
        * cd /usr/local/hnet/example/clientd
        * make

    * 客户端：（修改Makefile -D_USE_PROTOBUF_ 可打开protobuf功能）
        * cd /usr/local/hnet/example/chttp
        * make

* 运行（TCP）
    * 服务端
        * /usr/local/hnet/example/server/examplesvrd -h127.0.0.1 -p10025 -d

    * 客户端（单次）
        * /usr/local/hnet/example/client/exampleclient -h 127.0.0.7 -p 10025

    * 客户端（守护）
        * /usr/local/hnet/example/clientd/exampleclientd -h127.0.0.7 -p 10025

* 运行（HTTP）
    * 服务端
        * /usr/local/hnet/example/server/examplesvrd -h127.0.0.1 -p10025 -xhttp -d

    * 客户端（单次）
        * /usr/local/hnet/example/chttp/examplehttp -h 127.0.0.7 -p 10025 -xhttp

* 命令
    * 重启
        * /usr/local/hnet/example/server/examplesvrd -s restart

    * 停止
        * /usr/local/hnet/example/server/examplesvrd -s stop


[目录](../SUMMARY.md)

