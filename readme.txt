uuid生成器

运行：
	wxlistener ip port path/to/uuidworker [shmmemorysize]
	如：./wxlistener 0.0.0.0 9527 ./uuidworker 0

长度：
	64位(可配合mysql bigint类型使用)

格式：
	0|[毫妙时间戳:41位]|[时间点计数器:13位]|[分布式进程id:9位]

特性：
	1、在集群中为每个进程分配一个集群唯一的id，取值范围[0,1023]
	2、同一进程在同一毫秒可以生成uuid上限为16383个

配置：
	文件：uuidworker.conf
	1、如果有n个进程（根据cpu核心而定），则需要配置n个gpid，英文逗号分开，gpid取值[0,1023]
	2、connection=n 设置每个进程最多同时处理n个链接