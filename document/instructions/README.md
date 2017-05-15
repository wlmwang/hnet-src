# 注解

```
注意： example的四个示例从不同方向分别展示了HNET框架一般用法。请逐行仔细阅读！
```

* 通信协议（数据结构）
    * command/exampleCmd.h（example默认编译实际使用的数据结构）
    * 请求： example::ExampleReqEcho_t
    * 响应： example::ExampleResEcho_t
    *
    * message/example.proto（example提供的protobuf结构）
    * 请求： example.ExampleEchoReq
    * 响应： example.ExampleEchoRes

* 服务端：example/server/exampleServer.cpp
    * task客户端
        * ExampleChannelTask继承之wChannelTask，为Worker进程间同步类。构造函数中的On("example.ExampleEchoReq", &ExampleChannelTask::ExampleEchoReq, this);为事件注册。意为：当Worker进程接受到example.ExampleEchoReq（protobuf）消息的回调函数为ExampleChannelTask::ExampleEchoReq方法。

        * ExampleTcpTask继承之wTcpTask，为TCP客户端类。任何一个TCP客户端都将一一对应到ExampleTcpTask上。该构造函数中有两个事件注册方法On。这里使用了HNET多播功能：都为example.ExampleEchoReq的响应方法被注册时，系统会按顺序的依次调用。即当服务器接受到客户端发送example.ExampleEchoReq请求时，依次调用ExampleTcpTask::ExampleEchoReq、ExampleTcpTask::ExampleEchoChannel方法。

    * server服务端
        * ExampleServer继承之wServer，为服务器类。其中有两个虚函数：
            ExampleServer::NewTcpTask：当任一客户端连接到服务器时，服务器将调用该方法产生一个wTcpTask对象来跟踪该连接。写法固定。
            ExampleServer::NewChannelTask：服务器启动时，每产生一个Worker进程时，服务器将调用该方法产生一个wChannelTask对象来跟踪该进程。写法固定。（以单进程方式运行服务器时，该方法无用，也不用去实现）

    * main
        * 写法较为有代表性。值得注意的时，wConfig、wMaster用户均可再继承来实现更为定制化配置命令以及启动方式。

```
task客户端：每一个连接到服务器的客户端都将被虚拟为一个Task对象，在此示例中为ExampleTcpTask。
server服务端：每个服务器进程都有且仅有一个server对象，在此示例中为ExampleServer。
```


* example/client/exampleClient.cpp
    * 单次客户端
        * 请看注释

* example/clientd/exampleClientD.cpp
    * 守护客户端
        * ExampleTask方法说明如上
        * ExampleClient继承之wMultiClient，为TCP客户端类。该类主要应用在需要与服务器长期交互的地方，所以一般都会以线程方式运行，正如main中所示。当然该类自带断线重连机制。
            ExampleClient::NewTcpTask方法说明如上。即客户端连接服务器成功，会调用该方法产生一个wTcpTask对象来跟踪该连接。
            ExampleClient::PrepareRun为ExampleClient作为线程启动后调用的第一个调用的方法，此时可以做连接服务器工作，成功连接返回后，即进入客户端与服务器事件循环中。


```
注意：HNET的更多高级用法；完整综合应用，请查阅公司该基础产品：动态负载均衡-容错系统(HLBS)。
```

[目录](../SUMMARY.md)
