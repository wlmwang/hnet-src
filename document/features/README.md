# 特性

* 作为一个网络框架，HNET可方便架构出一个高性能、高可用、易扩展的服务端器。且其可同时监听多个端口。

* 作为一个基于Proactor模式的网络框架，HNET提供了方便且一致的事件注册、触发机制。

* 作为一个基于事件驱动、单线程的网络框架，HNET底层采用了目前较为流行且高效的I/O复用库epoll。

* 作为一个多协议的网络框架。在HNET中有以下两层含义：

    * 网络协议：目前系统已支持TCP SOCKET、UNIX DOMAIN SOCKET。可方便添加诸如HTTP、WebSocket等协议。构建时仅需传递相应的协议即可。

    * 数据协议：目前系统已支持自定义协议（C/C++中表现为结构体，系统中心跳机制使用（可关闭））、Protocol Buffer协议（https://github.com/google/protobuf ，系统中channel机制使用）。当然与客户端交互，建议使用protobuf，特别是不同语言间！

* 作为一个多进程的网络框架，HNET使用Master-Worker进程模式：

    * Master为管理进程。用来管理所有Worker进程和侦听所有注册信号，实现系统中Worker进程意外退出后重启；管理员的开启、停止和重启命令。

    * Worker为工作进程。用来侦听、接受和处理用户请求，是服务器的关键进程。默认情况下，Worker为主机CPU数量的两倍。
        * 另外值得注意的是：Worker进程间的同步也很重要！系统采用channel(socket pair)方式同步。为了一致性，HNET将该特性按照客户端编程规范实现。实战时你将意识到这有多么重要、方便！如系统中存在多个Worker中需全局共享的资源。（作者也是摸索了很多其他交互方式结果均不满意（有兴趣同学可翻看github提交记录，了解其他交互方式））

* 作为一个企业级框架，HNET是在实战的项目基础上演变而来的，不管是性能、容错、扩展还是其他功能都是经过考验的。HNET几乎在所有的关键点上都提供了接口（虚方法），给使用者足够的定制化自由，且HNET追求编码、架构的高度一致化：多协议中（网络协议、数据协议）的事件注册传播、触发事件后传递的回调参数、进程间通信与客户端服务器间通信、甚至是函数返回等都有所体现。

* 随库发布的还有单、多客户端基类，可直接使用。


```
    HNET在架构上借鉴了不少NGINX特性（熟悉NGINX的开发人员，有助于理解HNET），但两者毕竟从出发点就相去甚远，从而让两者不管从代码，提供的功能，开放的接口等都存在巨大差异。

    HNET主要侧重点是提供规范化的继承体系、一致的编程模式和极简的接口方法。
```
