#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <netinet/ip.h>
#include <poll.h>

int count = 0, max_fd = 0;
int ids[65536];
char *msgs[65536];
struct pollfd poll_fds[65536];
char buf_read[1001], buf_write[42];

// START COPY-PASTE FROM GIVEN MAIN

int extract_message(char **buf, char **msg)
{
	char *newbuf;
	int i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char *newbuf;
	int len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

// END COPY-PASTE

void fatal_error(void)
{
	write(2, "Fatal error\n", 12);
	exit(1);
}

void notify_other(int author, char *str)
{
	int len = strlen(str);

	for (int fd = 0; fd <= max_fd; fd++)
		if (poll_fds[fd].fd >= 0 && fd != author)
			send(fd, str, len, 0);
}

void register_client(int fd)
{
	max_fd = fd > max_fd ? fd : max_fd;
	ids[fd] = count++;
	msgs[fd] = NULL;
	poll_fds[fd].fd = fd;
	poll_fds[fd].events = POLLIN;
	sprintf(buf_write, "server: client %d just arrived\n", ids[fd]);
	notify_other(fd, buf_write);
}

void remove_client(int fd)
{
	sprintf(buf_write, "server: client %d just left\n", ids[fd]);
	notify_other(fd, buf_write);
	free(msgs[fd]);
	close(fd);
	poll_fds[fd].fd = -1;
	poll_fds[fd].events = 0;
}

void send_msg(int fd)
{
	char *msg;

	while (extract_message(&(msgs[fd]), &msg))
	{
		sprintf(buf_write, "client %d: ", ids[fd]);
		notify_other(fd, buf_write);
		notify_other(fd, msg);
		free(msg);
	}
}

int create_socket(void)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);

	if (fd < 0)
		fatal_error();
	max_fd = fd;
	poll_fds[fd].fd = fd;
	poll_fds[fd].events = POLLIN;
	return (fd);
}

int main(int ac, char **av)
{
	if (ac != 2)
	{
		write(2, "Wrong number of arguments\n", 26);
		exit(1);
	}

	int sockfd;
	struct sockaddr_in servaddr;

	for (int i = 0; i < 65536; i++)
		poll_fds[i].fd = -1;

	sockfd = create_socket();
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(2130706433);
	servaddr.sin_port = htons(atoi(av[1]));

	if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)))
		fatal_error();

	if (listen(sockfd, SOMAXCONN))
		fatal_error();

	while (1)
	{
		if (poll(poll_fds, max_fd + 1, -1) < 0)
			fatal_error();
		for (int fd = 0; fd <= max_fd; fd++)
		{
			if (poll_fds[fd].fd < 0 || !(poll_fds[fd].revents & POLLIN))
				continue;
			if (fd == sockfd)
			{
				socklen_t addr_len = sizeof(servaddr);
				int client_fd = accept(sockfd, (struct sockaddr *)&servaddr, &addr_len);
				if (client_fd >= 0)
					register_client(client_fd);
			}
			else
			{
				int read_bytes = recv(fd, buf_read, 1000, 0);
				if (read_bytes <= 0)
				{
					remove_client(fd);
					continue;
				}
				buf_read[read_bytes] = '\0';
				msgs[fd] = str_join(msgs[fd], buf_read);
				send_msg(fd);
			}
		}
	}
	return (0);
}
