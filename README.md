# quick

#### 介绍
quick是一个可靠的udp传输模块。为什么叫quick，很明显就是为了碰瓷quic协议。quick的设计参考了quic协议的一些概念，并且使用了chromium项目里面的bbr2算法来实现拥塞控制。
但quick并不是一个quic协议的实现，quick仅仅是一个可靠的、快速的、使用简单的基于udp的传输模块。quick支持windows、linux平台。
quic是一个高配版的可靠udp传输协议，它功能丰富强大，因而使用复杂，门槛较高，不是老司机一般都玩不转chromium下的quic模块。
而quick是一个吃丂版的可靠udp传输协议，它功能精简、应用简单，性能强悍，不论是老司机还是刚会写hello world的菜鸡，只要运行个exe测试一把，就能感受到udp传输的强大魅力。

#### quick设计
1.  提供客户端、服务端的模式。udp本身是无连接的传输协议，quick提供了连接的概念，客户端需要连接上服务器才能进行数据收发。
2.  支持连接迁移。服务端不再使用ip + port的方式来标识一个连接，而是使用客户端生成的32位随机数，只要客户端的这个随机数没有变化，服务器都认为这个连接是一个合法的连接。
3.  传输效率高。quick使用了定长数组、环形缓冲来缓存收发包，由于packet number类型为uint16_t，所以这些数组大小被设置为65536，刚好对应uint16_t类型数据的最大值，这样就可以使用packet number作为下标直接在数组中进行访问、插入、删除操作，效率高的一逼。因此quick在100Mbps的局域网内测试时，传输速率能轻松达到 11MB/s左右，同一主机上测试更是能够达到30~40MB/s。由于packet number是uint16_t类型的值，而uint16_t最大值仅为65535，在高速发送的情况下，paket number在短短2、3秒内就会发生环回，为了防止packet number环回导致数据包处理错误，quick内部进行了一些发送限制，所以quick的发送速度是有上限的，这个速度上限应该是40MB/s左右。
4.  使用单调递增的packet number。每个数据包，包括重传的包在发送时packet number都会加1，使用单调递增的packet number可以避免数据包的歧义。
5.  快重传。发送端发送序号1、2、3、4、5的包，当收到4、5包的ack消息而没有收到1、2、3的ack消息时，就认为1、2、3包是丢失了的，从而马上对1、2、3包进行重传。采用这样的设计是考虑到udp包的乱序出现几率远比丢包的几率要低，即使是真的出现了乱序，对这些乱序的包进行重传的代价也是可以接受的。
6.  使用bbr算法来进行拥塞控制。一个传输协议必须具备拥塞控制功能，否则在网络发生拥塞时，其传输效率将严重下降。quick采用了chromium里面的quic模块中的bbr2代码来实现拥塞控制，当网络带宽发生变化时，quick都能够调整到合适的发送速度，从而实现最优的网络吞吐。


#### 测试例子

1.  quic_client。一个采用qt实现界面的quic客户端
2.  quic_client_cmd。基于命令行的quick客户端，支持发送文件、循环发送消息功能。
3.  quic_server。一个采用qt实现界面的quic服务端
4.  quic_server_cmd。基于命令行的quick客户端，支持接收文件、接收数据功能。

#### 性能对比
kcp也是一个基于udp的可靠传输模块，在国内也算小有名气的，于是就将quick和tcp/kcp进行对比测试一番。
以下测试数据在局域网100Mbps环境下得到.

1.  不同的丢包率下传输速度对比:

![输入图片说明](https://images.gitee.com/uploads/images/2021/0826/100909_36aab6a8_8012401.png "1.png")
从测试数据可以看出无论是quick还是kcp，基于udp的可靠传输模块在抗网络丢包能力上，都明显优于tcp。
在无丢包或丢包率较低的网络环境下，quick的传输速度能甩开kcp两三条大街。

2.  带宽大小变化时模块调整到稳定的速度所需要时间

![输入图片说明](https://images.gitee.com/uploads/images/2021/0826/100928_3dc1ee91_8012401.png "2.png")

调整用时是指带宽大小发生变化时，由原来的传输速度调整到稳定的传输速度所用的时间。
在这项比拼上，quick也是独一档的的存在，而kcp就表现得像蜗牛一样，通道是我的测试方法有问题？


#### 联系/讨论
460775150@qq.com
