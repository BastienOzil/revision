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

void ft_error(void)
{
	write(2, "Fatal error\n", 12);
	exit(1);
}

void ft_talk(int author, char *str)
{
	int len = strlen(str);

	for (int fd = 0; fd <= max_fd; fd++)
		if (poll_fds[fd].fd >= 0 && fd != author)
			send(fd, str, len, MSG_NOSIGNAL);
}

void new_client(int fd)
{
	max_fd = fd > max_fd ? fd : max_fd;
	ids[fd] = count++;
	msgs[fd] = NULL;
	poll_fds[fd].fd = fd;
	poll_fds[fd].events = POLLIN;
	sprintf(buf_write, "server: client %d just arrived\n", ids[fd]);
	ft_talk(fd, buf_write);
}

void rm_client(int fd)
{
	sprintf(buf_write, "server: client %d just left\n", ids[fd]);
	ft_talk(fd, buf_write);
	free(msgs[fd]);
	close(fd);
	poll_fds[fd].fd = -1;
	poll_fds[fd].events = 0;
}

void ft_send(int fd)
{
	char *msg;

	while (extract_message(&(msgs[fd]), &msg))
	{
		sprintf(buf_write, "client %d: ", ids[fd]);
		ft_talk(fd, buf_write);
		ft_talk(fd, msg);
		free(msg);
	}
}

int ft_socket(void)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);

	if (fd < 0)
		ft_error();
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

	sockfd = ft_socket();
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(2130706433);
	servaddr.sin_port = htons(atoi(av[1]));

	if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)))
		ft_error();

	if (listen(sockfd, SOMAXCONN))
		ft_error();

	while (1)
	{
		if (poll(poll_fds, max_fd + 1, -1) < 0)
			ft_error();
		for (int fd = 0; fd <= max_fd; fd++)
		{
			if (poll_fds[fd].fd < 0 || !(poll_fds[fd].revents & POLLIN))
				continue;
			if (fd == sockfd)
			{
				socklen_t addr_len = sizeof(servaddr);
				int client_fd = accept(sockfd, (struct sockaddr *)&servaddr, &addr_len);
				if (client_fd >= 0)
					new_client(client_fd);
			}
			else
			{
				int read_bytes = recv(fd, buf_read, 1000, 0);
				if (read_bytes <= 0)
				{
					rm_client(fd);
					continue;
				}
				buf_read[read_bytes] = '\0';
				msgs[fd] = str_join(msgs[fd], buf_read);
				ft_send(fd);
			}
		}
	}
	return (0);
}
