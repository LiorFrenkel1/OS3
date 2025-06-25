//
// request.c: Does the bulk of the work for the web server.
//

#include "segel.h"
#include "request.h"

int append_stats(char* buf, threads_stats t_stats, struct timeval arrival, struct timeval dispatch){
    int offset = strlen(buf);  // Start after what's already written to buf

    offset += sprintf(buf + offset, "Stat-Req-Arrival:: %ld.%06ld\r\n",
                      arrival.tv_sec, arrival.tv_usec);

    offset += sprintf(buf + offset, "Stat-Req-Dispatch:: %ld.%06ld\r\n",
                      dispatch.tv_sec, dispatch.tv_usec);

    offset += sprintf(buf + offset, "Stat-Thread-Id:: %d\r\n",
                      t_stats->id);

    offset += sprintf(buf + offset, "Stat-Thread-Count:: %d\r\n",
                      t_stats->total_req);

    offset += sprintf(buf + offset, "Stat-Thread-Static:: %d\r\n",
                      t_stats->stat_req);

    offset += sprintf(buf + offset, "Stat-Thread-Dynamic:: %d\r\n",
                      t_stats->dynm_req);
    offset += sprintf(buf + offset, "Stat-Thread-Post:: %d\r\n\r\n",
                      t_stats->post_req);
    return offset;
}

// requestError(      fd,    filename,        "404",    "Not found", "OS-HW3 Server could not find this file");
void requestError(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg, struct timeval arrival, struct timeval dispatch, threads_stats t_stats)
{
    char buf[MAXLINE], body[MAXBUF];

    // Create the body of the error message
    sprintf(body, "<html><title>OS-HW3 Error</title>");
    sprintf(body, "%s<body bgcolor=""fffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr>OS-HW3 Web Server\r\n", body);

    // Write out the header information for this response
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    printf("%s", buf);

    sprintf(buf, "Content-Type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    printf("%s", buf);

    sprintf(buf, "Content-Length: %lu\r\n", strlen(body));

    int buf_len = append_stats(buf, t_stats, arrival, dispatch);

    Rio_writen(fd, buf, buf_len);
    printf("%s", buf);
    Rio_writen(fd, body, strlen(body));
    printf("%s", body);

}


void requestReadhdrs(rio_t *rp)
{
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    while (strcmp(buf, "\r\n")) {
        Rio_readlineb(rp, buf, MAXLINE);
    }
    return;
}

//
// Return 1 if static, 0 if dynamic content
// Calculates filename (and cgiargs, for dynamic) from uri
//
int requestParseURI(char *uri, char *filename, char *cgiargs)
{
    char *ptr;
    if (strstr(uri, "..")) {
        sprintf(filename, "./public/home.html");
        return 1;
    }
    if (!strstr(uri, "cgi")) {
        // static
        strcpy(cgiargs, "");
        sprintf(filename, "./public/%s", uri);
        if (uri[strlen(uri)-1] == '/') {
            strcat(filename, "home.html");
        }
        return 1;
    } else {
        // dynamic
        ptr = index(uri, '?');
        if (ptr) {
            strcpy(cgiargs, ptr+1);
            *ptr = '\0';
        } else {
            strcpy(cgiargs, "");
        }
        sprintf(filename, "./public/%s", uri);
        return 0;
    }
}

//
// Fills in the filetype given the filename
//
void requestGetFiletype(char *filename, char *filetype)
{
    if (strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if (strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else
        strcpy(filetype, "text/plain");
}

void requestServeDynamic(int fd, char *filename, char *cgiargs, struct timeval arrival, struct timeval dispatch, threads_stats t_stats)
{
    t_stats->dynm_req++;

    char buf[MAXBUF], *emptylist[] = {NULL};
    int offset = 0;

    offset += sprintf(buf + offset, "HTTP/1.0 200 OK\r\n");
    offset += sprintf(buf + offset, "Server: OS-HW3 Web Server\r\n");
    offset = append_stats(buf, t_stats, arrival, dispatch);

    Rio_writen(fd, buf, offset);

    int pid = Fork();
    if (pid == 0) {
        Setenv("QUERY_STRING", cgiargs, 1);
        Dup2(fd, STDOUT_FILENO);
        Execve(filename, emptylist, environ);
    }
    WaitPid(pid, NULL, 0);
}



void requestServeStatic(int fd, char *filename, int filesize, struct timeval arrival, struct timeval dispatch, threads_stats t_stats)
{
    // update the thread stats
    t_stats->stat_req++;

    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    requestGetFiletype(filename, filetype);

    srcfd = Open(filename, O_RDONLY, 0);

    // memory-map the file
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    Close(srcfd);

    // safely build the header using offset to avoid overlap
    int offset = 0;
    offset += sprintf(buf + offset, "HTTP/1.0 200 OK\r\n");
    offset += sprintf(buf + offset, "Server: OS-HW3 Web Server\r\n");
    offset += sprintf(buf + offset, "Content-Length: %d\r\n", filesize);
    offset += sprintf(buf + offset, "Content-Type: %s\r\n", filetype);
    offset = append_stats(buf, t_stats, arrival, dispatch);  // continues writing at the end

    Rio_writen(fd, buf, offset);
    Rio_writen(fd, srcp, filesize);
    Munmap(srcp, filesize);
}


void requestServePost(int fd, struct timeval arrival, struct timeval dispatch, threads_stats t_stats, server_log log)
{
    t_stats->post_req++;

    char header[MAXBUF], *body = NULL;
    int body_len = get_log(log, &body);
    int offset = 0;

    offset += sprintf(header + offset, "HTTP/1.0 200 OK\r\n");
    offset += sprintf(header + offset, "Server: OS-HW3 Web Server\r\n");
    offset += sprintf(header + offset, "Content-Length: %d\r\n", body_len);
    offset += sprintf(header + offset, "Content-Type: text/plain\r\n");
    offset = append_stats(header, t_stats, arrival, dispatch);

    Rio_writen(fd, header, offset);
    Rio_writen(fd, body, body_len);
    free(body);
}


// handle a request
void requestHandle(int fd, struct timeval arrival, struct timeval dispatch, threads_stats t_stats, server_log log)
{
    t_stats->total_req++;

    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;

    Rio_readinitb(&rio, fd);
    Rio_readlineb(&rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);


    if (!strcasecmp(method, "GET")) {
        requestReadhdrs(&rio);

        is_static = requestParseURI(uri, filename, cgiargs);
        if (stat(filename, &sbuf) < 0) {
            requestError(fd, filename, "404", "Not found",
                         "OS-HW3 Server could not find this file",
                         arrival, dispatch, t_stats);
            return;
        }

        if (is_static) {
            if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
                requestError(fd, filename, "403", "Forbidden",
                             "OS-HW3 Server could not read this file",
                             arrival, dispatch, t_stats);
                return;
            }

            requestServeStatic(fd, filename, sbuf.st_size, arrival, dispatch, t_stats);

            char statBuffer[MAXBUF];
            statBuffer[0] = '\0';
            int statLength = append_stats(statBuffer, t_stats, arrival, dispatch);
            add_to_log(log, statBuffer, statLength);

        } else {
            if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
                requestError(fd, filename, "403", "Forbidden",
                             "OS-HW3 Server could not run this CGI program",
                             arrival, dispatch, t_stats);
                return;
            }

            requestServeDynamic(fd, filename, cgiargs, arrival, dispatch, t_stats);

            char statBuffer[MAXBUF];
            statBuffer[0] = '\0';
            int statLength = append_stats(statBuffer, t_stats, arrival, dispatch);
            add_to_log(log, statBuffer, statLength);
        }

    } else if (!strcasecmp(method, "POST")) {
        requestServePost(fd, arrival, dispatch, t_stats, log);

    } else {
        requestError(fd, method, "501", "Not Implemented",
                     "OS-HW3 Server does not implement this method",
                     arrival, dispatch, t_stats);
        return;
    }
}