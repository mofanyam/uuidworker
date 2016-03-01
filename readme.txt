uuid生成器

运行：
	tcplistener ip port path/to/uuidworker processnumber shmmemorysize
	如：./tcplistener 0.0.0.0 9527 ./uuidworker 2 0

长度：
	64位(可配合mysql bigint类型使用)

格式：
	0|[分布式进程id:9位]|[毫妙时间戳:41位]|[时间点计数器:13位]

特性：
	1、在集群中为每个进程分配一个集群唯一的id，取值范围[0,1023]
	2、同一进程在同一毫秒可以生成uuid上限为16383个