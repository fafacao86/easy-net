## 随记

* 封装了系统调用，可移植性强，通用链表使用Linux Kernel里的风格 https://kernelnewbies.org/FAQ/LinkedLists
* 网卡读写线程与核心工作线程之间，用信号量实现了生产者消费者消息队列，这里的队列的目的主要是降低多线程编程的难度，应用层的线程和网卡收发数据的线程，都通过msg给工作线程传命令，协议栈内部的数据结构就只会被工作线程访问
* 用静态内存池管理内存，可以用来管理底层消息，也可以用来管理网络包的申请与释放，一切需要申请与释放的结构体都可以用静态内存池来管理
* 使用分页式数据包，便于包头的添加与释放，提供类似于文件系统VFS的offset读写API
* 接口与多态：
  * netif抽象了各种各样的物理网卡，包括环回网卡
  * socket API仿照Linux中的实现，使用sock结构将不同类型的socket（RAW, STREAM, DATAGRAM）操作抽象出来实现多态
  * 消费者线程在调用协议栈接口时，使用统一的blocking stub
* ARP缓存表使用链表实现，访问到的节点，就移到链表头，可以做到越常用的表项，越靠前，查找越快

* C语言bit field是个好东西，操作数据包头非常方便，但是一定要注意大小端的问题，bit field内部也会有大小端的问题
* 用结构体定义header的时候，用编译器指令，pragma pack(1)强制禁用对齐
* EthernetII 的负载居然有最小值的限制，46-1500，如果上层协议不够46字节，需要自己加padding，比如ARP
* 好奇ethernet帧没有size字段，那是怎么分包的。疑似是底层网卡自己维护信息，协议栈不用管，网卡提供的API就是read next packet
* MTU是单纯的payload大小，不包括header

- 免费ARP（gratuitous ARP）的sender IP和target IP一样，协议栈启动的时候需要发送免费ARP
- ARP在等待解析的过程中，如果此时上层有发包，不要丢，把包挂在arp entry上，等响应回来了再发出去
- 广播分为定向广播和本地广播

- IP的校验和只覆盖header，上层协议需要自己对数据做checksum
- IP头部里offset只有13位，所以RFC规定，offset的基本单位是8bytes，所以在做分组发送的时候必须做到非最后一个fragment，offset一定是8字节对齐。MTU为1500的时候是不会有这个问题的，因为1480可以被8整除，但是如果你改了MTU，就会有bug

- ICMP的32位保留字段，在ping中分为了两个16位数，id和seq，分别用来区分同一台机器上的不同ping进程，同一ping进程发起的不同ping request

- UDP发送时可以不指定本地端口，协议栈会自动随机分配
- UDP socket的connect和bind其实只是简单的设置remote_ip, remote_port, local_ip, local_port

- UDP socket接收和bind的时候，可以指定ip地址为全0，相当于监听协议栈中所有网卡。全零相当于wildcard

- MSS是TCP选项，在连接建立时协商，如果不指定，MSS为536，因为576是RFC指定的网络设备最小必须支持的最大IPv4数据包大小。注意，MSS包括了TCP header的大小

- TCP常用选项有MSS，SACK，WSCALE，加上了Window Scaling的滑动窗口最高可达1个G，不加的话最大64kb

- 关于RST，只需要遵循一个基本原则，TCP连接本质是状态机，只要你收到了你在当前状态不应该收到的包，就给对面回一个RST，一些常见情况：
  - https://www.ibm.com/support/pages/tcpip-sockets-reset-concerns
  - https://stackoverflow.com/questions/251243/what-causes-a-tcp-ip-reset-rst-flag-to-be-sent
  - 不在CLOSE_WAIT状态调用close()

- 就算接收方窗口为0也要处理RST

- TCP三次握手的原因：先假设如果只有两次握手，那只要remote收到SYN，remote就会进入建立成功的状态，这样很容易造成资源浪费，比如历史SYN和SYN攻击。然后就是连接建立的时候需要协商一些参数，需要有来有回，比如MSS，ISN，SACK等

- 涉及到FIN，一定要小心处理FIN的乱序，FIN可能比数据先到达，确认FIN之前的数据是不是都收到了再切换状态，不然会丢数据。发送FIN的时候要等发送缓冲区发完了再发FIN

- TCP是可以三次挥手的，把FIN和ACK并在一起（TCP连接状态图里是有这样一条边的），主动关闭方就直接从FIN_WAIT_1跳过FIN_WAIT_2进入TIME_WAIT。在实现FIN_WAIT_1时小心：simultaneous close和三次挥手的处理

- 如果对面关闭了连接，就不能再从对面读出数据了，这句话指的是协议栈收到对面的数据包会直接扔了，但是作为用户还是可以对半关闭的socket调用recv，此时返回0即可，这里在实现recv时要区分所处状态

- 当对面发送速率特别快的时候，我们要减小窗口大小，如果窗口大小减为零，抓包发现对面并不是完全不发了，服务器会发ZeroWindowProbe报文一直来试探，一次一个字节

- RFC1122要求keep-alive的间隔最少默认时间必须在2小时以上

- listen函数只是单纯地设置backlog和切换状态为LISTEN，LISTEN状态下收到RST不要abort，直接忽略就好

* 这里用父子TCP来实现连接队列，当接收到SYN，listen态的socket spawn一个未激活的socket，等到了ESTABLISHED且用户调用accept，变为激活态

- 通过TCP sender状态机来实现超时重传，不用状态机的话一旦结合上超时重传，transmit函数会很难写


* 在使用memory_pool的时候，block的大小一定要比list_node_t大
* 申请pages的时候如果申请到一半失败，要把申请到的不完整的pages给free掉，所有的alloc过程，如果执行到一半失败，一定要把前半部分申请成功的资源释放

- 定时器链表在扫描时，不要扫描到一个超时的就立即去执行，因为可能执行函数里可能也会有对定时器的操作，这样会造成，一边遍历，一边修改链表的问题。应该扫描的时候存下超时的所有节点
- 在做IP分组重组时，insert packet的时候忘了检查是否已经存在，这就会导致如果一个包被发了两次，会重复

- 发RST前检查要回复的报文是不是RST，不要对RST报文回RST报文，不然死循环

- 在算seq的时候，SYN和FIN在逻辑上各占一个字节 (tcp_ack_process里踩过坑)

* 在计算窗口时，小心回绕，比较两个SEQ，哪个在左，哪个在右，要考虑回绕，这里直接用TCP/IP详解里的结论，把SEQ a和SEQ b当作有符号整数，a-b<0则a在b的右边

- 宏一定要加括号，debug半天

- 用宏undef防止操作系统协议栈函数与自定义函数/宏冲突
