#include "stdafx.h"
#include <errno.h>
#include <signal.h>
#include <iostream>
#include <string>
//#include <windows.h>
//#include <winsock2.h>
//libevent��
#include "event2/event.h"
#include "event2/bufferevent.h"
#include "event2/buffer_compat.h"
#include "event2/listener.h"
#include "event2/buffer.h"

static const char MESSAGE[] = "Hello, NewConnection!\n";
static const int PORT = 9995;

static void conn_writecb(struct bufferevent *bev, void *user_data)
{
	//ȡ��bufferevent ��output����
	struct evbuffer *output = bufferevent_get_output(bev);
	//����Ϊ0����ôд��ϣ��ͷſռ�
	if (evbuffer_get_length(output) == 0)
	{
		printf("flushed answer\n");
		bufferevent_free(bev);
	}
}

//������Ϊ�¼��ص�������д�Լ���Ҫ���Ĺ��ܾ���
//���ǵ��ͷ�buffevent�ռ�
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
	//����һ��bufferevent�����ڶ�����д
	bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
	if (!bev)
	{
		fprintf(stderr, "Error constructing bufferevent!");
		event_base_loopbreak(base);
		return;
	}
	//������д�ص��������¼��Ļص�����
	bufferevent_setcb(bev, NULL, conn_writecb, conn_eventcb, NULL);
	//bufferevent����д�¼��ص�
	bufferevent_enable(bev, EV_WRITE);
	//bufferevent�رն��¼��ص�
	bufferevent_disable(bev, EV_READ);
	//��MESSAGE�ַ���������outbuffer��
	bufferevent_write(bev, MESSAGE, strlen(MESSAGE));
}

//����׽���źź����baseloop��ֹ
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
	//����event_base
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

	//����eventbase ����listen����������
	//������listener_cb�ص������������µ����ӵ�¼��ʱ��
	//����listener_cb
	listener = evconnlistener_new_bind(base, listener_cb, (void *)base,
		LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, -1,
		(struct sockaddr*)&sin,
		sizeof(sin));

	if (!listener)
	{
		fprintf(stderr, "Could not create a listener!\n");
		return 1;
	}

	//�����ն��źţ��������յ�SIGINT�����signal_cb
	signal_event = evsignal_new(base, SIGINT, signal_cb, (void *)base);

	if (!signal_event || event_add(signal_event, NULL)<0)
	{
		fprintf(stderr, "Could not create/add a signal event!\n");
		return 1;
	}
	//event_base��Ϣ�ɷ�
	event_base_dispatch(base);

	//�ͷ����ɵ�evconnlistener
	evconnlistener_free(listener);
	//�ͷ����ɵ��ź��¼�
	event_free(signal_event);
	//�ͷ�event_base
	event_base_free(base);

	system("\npause");
	return 0;
}

