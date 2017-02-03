/*
 * proxy.c - CS:APP Web proxy
 *
 * TEAM MEMBERS:
 *     Andrew Carnegie, ac00@cs.cmu.edu 
 *     Harry Q. Bovik, bovik@cs.cmu.edu
 *
 * Modified by: Priyanka Chordia.
 * 
 * IMPORTANT: Give a high level description of your code here. You
 * must also provide a header comment at the beginning of each
 * function that describes what that function does.
 */ 

#include "csapp.h"

/*
 * Function prototypes
 */
int parse_uri(char *uri, char *target_addr, char *path, int  *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size);
void *thread(void *arg);
int process(int thread_id, int conn_fd, struct sockaddr_in);
void error(int file_d, char *errno, char *msg);
int Open_client(char *hostname, int port);

sem_t gethost;
sem_t getlog;
sem_t client;

ssize_t Rio_writen_r(int fd, void *usrbuf, size_t n);
void Rio_readinitb_r(rio_t *rp, int fd);
ssize_t Rio_readnb_r(rio_t *rp, void *usrbuf, size_t n);
ssize_t Rio_readlineb_r(rio_t *rp, void *usrbuf, size_t maxlen);

FILE *filename;

/* 
 * main - Main routine for the proxy program 
 */
int main(int argc, char **argv)
{
    signal(SIGPIPE,SIG_IGN);

    pthread_t pid;
    int listenfd, port;
    int *conn_fd;
    struct sockaddr_in clientaddr;
    socklen_t client_length;

    /* Check arguments */
    if (argc != 2) {
    fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
    exit(0);
    }

    Sem_init(&gethost,0,1);
    Sem_init(&getlog,0,1);
    
    port = atoi(argv[1]);
    listenfd = Open_listenfd(port);
    if(listenfd == 0)
    {
        fprintf(stderr, "Error occured for listenfd at port %s\n", argv[0]);
    }
    while(1)
    {
        client_length = sizeof(clientaddr);
        conn_fd = Malloc(sizeof(int) + sizeof(struct sockaddr_in) + 8);
        *conn_fd = Accept(listenfd, (SA *)&clientaddr, &client_length);
        *(struct sockaddr_in*)(conn_fd + 1) = clientaddr;
        Pthread_create(&pid, NULL, thread, (void*)(conn_fd));
    }
    fclose(filename);
    return 0;
}


/*
 * parse_uri - URI parser
 * 
 * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
 * the host name, path name, and port.  The memory for hostname and
 * pathname must already be allocated and should be at least MAXLINE
 * bytes. Return -1 if there are any problems.
 */
int parse_uri(char *uri, char *hostname, char *pathname, int *port)
{
    char *hostbegin;
    char *hostend;
    char *pathbegin;
    int len;

    if (strncasecmp(uri, "http://", 7) != 0) {
    hostname[0] = '\0';
    return -1;
    }
       
    /* Extract the host name */
    hostbegin = uri + 7;
    hostend = strpbrk(hostbegin, " :/\r\n\0");
    len = hostend - hostbegin;
    strncpy(hostname, hostbegin, len);
    hostname[len] = '\0';
    
    /* Extract the port number */
    *port = 80; /* default */
    if (*hostend == ':')   
    *port = atoi(hostend + 1);
    
    /* Extract the path */
    pathbegin = strchr(hostbegin, '/');
    if (pathbegin == NULL) {
    pathname[0] = '\0';
    }
    else {
    pathbegin++;    
    strcpy(pathname, pathbegin);
    }

    return 0;
}

/*
 * format_log_entry - Create a formatted log entry in logstring. 
 * 
 * The inputs are the socket address of the requesting client
 * (sockaddr), the URI from the request (uri), and the size in bytes
 * of the response from the server (size).
 */
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, 
              char *uri, int size)
{
    P(&getlog);

    filename = fopen("p_log.txt","a+");
    time_t now;
    char time_str[MAXLINE];
    unsigned long host;
    unsigned char a, b, c, d;

    /* Get a formatted time string */
    now = time(NULL);
    strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));

    /* 
     * Convert the IP address in network byte order to dotted decimal
     * form. Note that we could have used inet_ntoa, but chose not to
     * because inet_ntoa is a Class 3 thread unsafe function that
     * returns a pointer to a static variable (Ch 13, CS:APP).
     */
    host = ntohl(sockaddr->sin_addr.s_addr);
    a = host >> 24;
    b = (host >> 16) & 0xff;
    c = (host >> 8) & 0xff;
    d = host & 0xff;


    /* Return the formatted log entry string */
    sprintf(logstring, "%s: %d.%d.%d.%d %s", time_str, a, b, c, d, uri);
    fprintf(filename, "%s %d\n",logstring, size);
    fflush(filename);
    fclose(filename);
    V(&getlog);
}

void* thread(void* arg)
{
    int conn_fd = *(int*)arg;
    struct sockaddr_in clientaddr = *(struct sockaddr_in*)((int*)arg + 1);
    fflush(stdout);
    Pthread_detach(pthread_self());
    process(pthread_self(), conn_fd, clientaddr);
    Free(arg);
    return NULL;
}

int process(int pid, int conn_fd, struct sockaddr_in clientaddr)
{
    char buf[MAXLINE], buf2[MAXLINE];
    char hostname[MAXLINE],pathname[MAXLINE],uri[MAXLINE],uri2[MAXLINE];
    char method[MAXLINE], version[MAXLINE];
    rio_t rio_web;
    rio_t rio_client;
    int fd_web, port_web, length;
    int currentLength = 0;
    int fd_i;

    Rio_readinitb(&rio_client, conn_fd);
    char *logstring = malloc(sizeof(char)*MAXLINE);

    length = Rio_readlineb(&rio_client, buf, MAXLINE);

    if(length != 0)
        strcpy(buf2,buf);
    else 
        return -2;

    sscanf(buf2, "%s %s %s",method, uri, version);
    strcpy(uri2,uri);
    parse_uri(uri, hostname, pathname, &port_web);
    fd_i = 0;
    while(fd_i < 3)
    {
        fd_web = Open_client(hostname, port_web);
        if(fd_web != -1)
            break;
        fd_i ++;
    }
    if(fd_web == -1)
    {
        printf("DNS ERROR\n");
        close(conn_fd);
        return -1;
    }
    Rio_readinitb(&rio_web, fd_web);
    Rio_writen_r(fd_web, buf, length);

#ifdef DEBUG_PIC
    printf("tid_%d, Request Header\n");
#endif

    while( (length = Rio_readlineb(&rio_client,buf, MAXLINE)) )
    {
#ifdef DEBUG_PIC3
        printf("tid_%d, %s\n",tid, buf);
#endif
        if ((strstr(buf, "Proxy-Connection:") == NULL) )
        {
            Rio_writen_r(fd_web, buf, length);
        }
        else
        {
            char* temp = "Connection: close\r\n";
            Rio_writen_r(fd_web, temp, strlen(temp));
        }

        if(strcmp(buf,"\r\n") == 0)
        {
            break;
        }
    }
#ifdef DEBUG_PIC
    printf("tid_%d Header complete\n",tid);

    printf("tid_%d, Response Header\n");
#endif
    
    while( (length = Rio_readnb_r(&rio_web, buf, MAXLINE) ) )
    {
        Rio_writen_r(conn_fd, buf, length);
        currentLength += length;
#ifdef DEBUG_PIC
        printf("tid_%d, %s\n",tid, buf);
#endif
        if(strcmp(buf,"\r\n") == 0)
        {
            break;
        }
    }
#ifdef DEBUG_PIC
    printf("tid_%d Response complete\n",tid);

    printf("tid_%d, Content\n");
#endif

    while( (length = Rio_readlineb_r(&rio_web,buf, MAXLINE)) )
    {
#ifdef DEBUG_PIC
        printf("tid_%d length = %d, %s\n",tid, length, buf);
#endif
        Rio_writen_r(conn_fd, buf, length);
        currentLength += length;
    }
#ifdef DEBUG_PIC
    printf("tid_%d close, received %d bytes\n",tid, currentLength);
    fflush(stdout);
#endif
    format_log_entry(logstring, &clientaddr,uri2, currentLength);
    close(fd_web);
    close(conn_fd);

    return currentLength;
}

void error(int file_d, char *errno, char *msg)
{
    char buf[MAXLINE], body[MAXBUF];

    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errno, msg);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    sprintf(buf, "HTTP/1.0 %s %s\r\n", errno, msg);
    Rio_writen_r(file_d, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen_r(file_d, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen_r(file_d, buf, strlen(buf));
    Rio_writen_r(file_d, body, strlen(body));
}

int Open_client(char* hostname, int port)
{
    int client_fd;
    struct hostent *hp;
    struct sockaddr_in serveraddr;

    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    return -1; 
    P(&gethost);

    if ((hp = gethostbyname(hostname)) == NULL)
    {
        V(&gethost);
        return -2; 
    }
    V(&gethost);
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)hp->h_addr_list[0],(char *)&serveraddr.sin_addr.s_addr, hp->h_length);
    serveraddr.sin_port = htons(port);

    if (connect(client_fd, (SA *) &serveraddr, sizeof(serveraddr)) < 0)
        return -1;
    return client_fd;       
}

ssize_t Rio_readlineb_r(rio_t *rp, void *usrbuf, size_t maxlen)
{
    ssize_t rc;

    if ((rc = rio_readlineb(rp, usrbuf, maxlen)) < 0)
        return -1;
    return rc;
}

ssize_t Rio_writen_r(int fd, void *usrbuf, size_t n)
{
    if (rio_writen(fd, usrbuf, n) != n)
        return -1;
    return n;
}

ssize_t Rio_readnb_r(rio_t *rp, void *usrbuf, size_t n)
{
    ssize_t rc;

    if ((rc = rio_readnb(rp, usrbuf, n)) < 0)
        return -1;
    return rc;
}

void Rio_readinitb_r(rio_t *rp, int fd)
{
    rio_readinitb(rp, fd);
}