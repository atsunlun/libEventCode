#include "stdafx.h"
#include <errno.h>
#include <signal.h>
#include <iostream>
#include <string>
//#include <windows.h>
//#include <winsock2.h>
//libevent库
#include "event2/event.h"
#include "event2/bufferevent.h"
#include "event2/buffer_compat.h"
#include "event2/listener.h"
#include "event2/buffer.h"

static const char MESSAGE[] = "Hello, NewConnection!\n";
static const int PORT = 9995;

static void conn_writecb(struct bufferevent *bev, void *user_data)
{
	//取出bufferevent 的output数据
	struct evbuffer *output = bufferevent_get_output(bev);
	//长度为0，那么写完毕，释放空间
	if (evbuffer_get_length(output) == 0)
	{
		printf("flushed answer\n");
		bufferevent_free(bev);
	}
}

//仅仅作为事件回调函数，写自己想要做的功能就行
//最后记得释放buffevent空间
static void conn_eventcb(struct bufferevent *bev, short events, void *user_data)
{
	if (events & BEV_EVENT_EOF)
	{
		printf("Connection closed.\n");
	}
	else if (events & BEV_EVENT_ERROR)
	{
		printf("Got an error on the connection: %s\n",
			strerror(errno));/*XXX win32*/
	}
	/* None of the other events can happen here, since we haven't enabled
	* timeouts */
	bufferevent_free(bev);
}

static void listener_cb(struct evconnlistener *listener, evutil_socket_t fd,
struct sockaddr *sa, int socklen, void *user_data)
{
	struct event_base *base = (struct event_base *)user_data;
	struct bufferevent *bev;
	//生成一个bufferevent，用于读或者写
	bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
	if (!bev)
	{
		fprintf(stderr, "Error constructing bufferevent!");
		event_base_loopbreak(base);
		return;
	}
	//设置了写回调函数和事件的回调函数
	bufferevent_setcb(bev, NULL, conn_writecb, conn_eventcb, NULL);
	//bufferevent设置写事件回调
	bufferevent_enable(bev, EV_WRITE);
	//bufferevent关闭读事件回调
	bufferevent_disable(bev, EV_READ);
	//将MESSAGE字符串拷贝到outbuffer里
	bufferevent_write(bev, MESSAGE, strlen(MESSAGE));
}

//程序捕捉到信号后就让baseloop终止
static void signal_cb(evutil_socket_t sig, short events, void *user_data)
{
	struct event_base *base = (struct event_base *)user_data;
	struct timeval delay = { 2, 0 };

	printf("Caught an interrupt signal; exiting cleanly in two seconds.\n");

	event_base_loopexit(base, &delay);
}

int _tmain(int argc, _TCHAR* argv[])
{
	struct event_base *base;
	struct evconnlistener *listener;
	struct event *signal_event;

	struct sockaddr_in sin;

#ifdef WIN32
	WSADATA wsa_data;
	WSAStartup(0x0201, &wsa_data);
#endif
	//创建event_base
	base = event_base_new();
	if (!base)
	{
		fprintf(stderr, "Could not initialize libevent!\n");
		return 1;
	}

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(PORT);
	sin.sin_addr.s_addr = inet_addr("192.168.1.99");
	std::string ipstr = inet_ntoa(sin.sin_addr);
	std::cout << ipstr.c_str();

	//基于eventbase 生成listen描述符并绑定
	//设置了listener_cb回调函数，当有新的连接登录的时候
	//触发listener_cb
	listener = evconnlistener_new_bind(base, listener_cb, (void *)base,
		LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, -1,
		(struct sockaddr*)&sin,
		sizeof(sin));

	if (!listener)
	{
		fprintf(stderr, "Could not create a listener!\n");
		return 1;
	}

	//设置终端信号，当程序收到SIGINT后调用signal_cb
	signal_event = evsignal_new(base, SIGINT, signal_cb, (void *)base);

	if (!signal_event || event_add(signal_event, NULL)<0)
	{
		fprintf(stderr, "Could not create/add a signal event!\n");
		return 1;
	}
	//event_base消息派发
	event_base_dispatch(base);

	//释放生成的evconnlistener
	evconnlistener_free(listener);
	//释放生成的信号事件
	event_free(signal_event);
	//释放event_base
	event_base_free(base);

	system("\npause");
	return 0;
}

