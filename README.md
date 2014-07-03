使用方法
----------------------------
<pre>
> usage: http-scope [-c config] [-fd]
> 
> Options:
>  -c <config_file>  /usr/local//etc/bscope.conf
>  -p <pid_file>
>  -i <network_interface>    eth0/eth1....
>  -f console mode
>  -d daemon mode
>  -0 disable supervisor process
>  -v show version
>  -h show help
> 
> Tools:
>  -l show logs
>  -s show status
</pre>

配置文件
----------------------------
```
[master]
host=127.0.0.1
port=5672
user=guest
password=guest
vhost=/

[http-scope]
daemon-mode = yes
pid-file = /var/run/bscope.pid
network_interface = eth0

node-name = default
;host-name = localhost google.com scm.ec-ae.com
host-port = 80
packet-checksum=no
host-ip = 0.0.0.0/0
catch-content-by-code= 5xx
catch-request-header = user-agent referer accept
catch-response-header = server host cookie x-powered-by content-type
catch-cookie = ASPSESSID SESSID PHPSESSID 
catch-get = *
catch-post = *
```


topic规则:
------------------------------
routing key = http-scope.www.example.com.2xx.200

例子:
1. 所有google的请求(包括各个子域名):   http-scope.#.google.com.#
1. 所有成功的请求 http-scope.#.2xx.*
1. 所有504的错误 http-scope.#.5xx.504