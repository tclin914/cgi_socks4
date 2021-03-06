#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>

#define F_CONNECTING 0
#define F_READING    1
#define F_WRITING    2
#define F_DONE       3

#define HEAD "<html>\r\n<head>\r\n<meta http-equiv=\"Content-Type\" content=\"text/html; charset=big5\" />\r\n<title>Network Programming Homework 3</title>\r\n</head>\r\n"
#define BODY "<body bgcolor=#336699>\r\n"
#define FONT "<font face=\"Courier New\" size=2 color=#FFFF99>\r\n"
#define TABLE1 "<table width=\"800\" border=\"1\">\r\n<tr>\r\n"
#define TABLE_ELEMENT "<td>%s</td>\r\n"
#define TABLE2 "</tr>\r\n<tr>\r\n<td valign=\"top\" id=\"m0\"></td><td valign=\"top\" id=\"m1\"></td><td valign=\"top\" id=\"m2\"></td>\r\n<td valign=\"top\" id=\"m3\"></td><td valign=\"top\" id=\"m4\"></td>\r\n</tr>\r\n</table>\r\n"
#define SCRIPT_MESSAGE "<script>document.all[\'%s\'].innerHTML += \"%s<br>\";</script>\r\n"
#define SCRIPT_COMMAND "<script>document.all[\'%s\'].innerHTML += \"% <b>%s</b><br>\";</script>\r\n"
#define TAIL "</font>\r\n</body>\r\n</html>\r\n"

const char *mlist[5] = {
    "m0",
    "m1",
    "m2",
    "m3",
    "m4"
};

void parseString(char* string, int* nbHost);
int readline(int fd,char *ptr,int maxlen);
int printMsg(const int i, char* string);
char* hosts[5];
char* ports[5];
char* files[5];
char* socksip[5];
char* socksport[5];
FILE* filefps[5];

int main(int argc,char *argv[])
{
    int nbHost = 0;
    char* queryString = getenv("QUERY_STRING");
    parseString(queryString, &nbHost);

    printf(HEAD);
    printf(BODY);
    printf(FONT);
    printf(TABLE1);
    fflush(stdout);

    for (int i = 0; i < nbHost; i++) {
        printf(TABLE_ELEMENT, hosts[i]);
        fflush(stdout);
    }

    printf(TABLE2);
    printf(TAIL);
    fflush(stdout);

    for (int i = 0; i < nbHost; i++) {
        filefps[i] = fopen(files[i], "r");
    }
    
    int n;
    int socks_fd[5] = { 0};
    struct sockaddr_in socks_sin[5];
    memset(&socks_sin, 0, sizeof(socks_sin));
    for (int i = 0; i < nbHost; i++) {
        socks_fd[i] = socket(AF_INET, SOCK_STREAM, 0);
        bzero(&socks_sin[i], sizeof(socks_sin[i]));
        socks_sin[i].sin_family = AF_INET;
        socks_sin[i].sin_addr.s_addr = inet_addr(socksip[i]);
        socks_sin[i].sin_port = htons(atoi(socksport[i]));
        
        int flags = fcntl(socks_fd[i], F_GETFL, 0);
        fcntl(socks_fd[i], F_SETFL, flags | O_NONBLOCK);
        
        if ((n = connect(socks_fd[i], (struct sockaddr*)&socks_sin[i], sizeof(socks_sin[i]))) < 0) {
            if (errno != EINPROGRESS) 
                printf("connect error\n");
        }
    }

    unsigned char request[11];
    memset(request, 0, 11);

    fd_set rfds;
    fd_set wfds;
    fd_set rs;
    fd_set ws;

    int conn = nbHost;
    int nfds = FD_SETSIZE;
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    FD_ZERO(&rs);
    FD_ZERO(&ws);
   
    for (int i = 0; i < nbHost; i++) {
        FD_SET(socks_fd[i], &rs);
        FD_SET(socks_fd[i], &ws);
    }
    
    rfds = rs;
    wfds = ws;
    
    n = sizeof(int);
    int statuses[5] = { F_CONNECTING};
    int status;
    int fd;
    int i;
    int error;
    char recv_buf[2048];
    char command_buf[1024];
    while (conn > 0) {
        memcpy(&rfds, &rs, sizeof(rfds));
        memcpy(&wfds, &ws, sizeof(wfds));

        if (select(nfds, &rfds, &wfds, (fd_set*)0, (struct timeval*)0) < 0) printf("select error\n");
        
        for (i = 0; i < 5; i++) {
            status = statuses[i];
            fd = socks_fd[i];
            
            if (status == F_CONNECTING && (FD_ISSET(fd, &rfds) || FD_ISSET(fd, &wfds))) {
                if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (void*)&error, &n) < 0 || error != 0) {
                    printf("non-blocking error\n");
                }

                request[0] = 4;
                request[1] = 1;
                request[2] = (unsigned char)(atoi(ports[i]) / 255);
                request[3] = (unsigned char)(atoi(ports[i]) % 256);
                sscanf(hosts[i], "%hhu.%hhu.%hhu.%hhu", &(request[4]), &(request[5]), &(request[6]), 
                        &(request[7]));

                n = write(fd, request, 8);
                
                statuses[i] = F_READING;
                FD_CLR(socks_fd[i], &ws);  
            } else if (status == F_WRITING && FD_ISSET(fd, &wfds)) {
                memset(command_buf, 0, 1024);
                int len = readline(fileno(filefps[i]),  command_buf, sizeof(command_buf)); 
                // recogniza 13 and 10
                int c = 0;
                while (command_buf[len - 1 - c] == 13 || command_buf[len - 1 - c] == 10) {
                    c++;
                }
                command_buf[len - c] = '\0';
                printf(SCRIPT_COMMAND, mlist[i], command_buf); 
                fflush(stdout);
                command_buf[len - 1] = 13; 
                command_buf[len] = 10; 
                command_buf[len + 1] = '\0'; 
                n = write(fd, command_buf, len + 1); 
                if (n > 0) {
                    statuses[i] = F_READING;
                    FD_CLR(socks_fd[i], &ws);
                    FD_SET(socks_fd[i], &rs);
                }
            } else if (status == F_READING && FD_ISSET(fd, &rfds)) {
                memset(recv_buf, 0, 2048);
                n = read(fd, recv_buf, sizeof(recv_buf) - 1);
                // recognize 13 and 10
                int c = 1;
                while (recv_buf[n - 1 - c] == 13 || command_buf[n - 1 - c] == 10) {
                    c++;                        
                }
                recv_buf[n - c] = '\0';
                if (n > 0) {
                    printMsg(i, recv_buf);
                    /* if (printMsg(i, recv_buf) == 1) {  */
                    if (recv_buf[strlen(recv_buf) - 2] == '%') {
                        statuses[i] = F_WRITING;
                        FD_CLR(socks_fd[i], &rs);
                        FD_SET(socks_fd[i], &ws);
                    }
                } else {
                    close(socks_fd[i]);
                    FD_CLR(socks_fd[i], &rs);
                    statuses[i] = F_DONE;
                    conn--;
                }
            }
        }
    }

    return 0;
}

int readline(int fd,char *ptr,int maxlen)
{
  int n, rc;
  char c;
  *ptr = 0;
  for(n = 1; n < maxlen; n++)
  {
    rc = read(fd,&c,1);
    if(rc == 1)
    {
      *ptr++ = c;
      if(c =='\n')  break;
    }
    else if(rc ==0)
    {
      if(n == 1)     return 0;
      else         break;
    }
    else return (-1);
  }
  return n;
}      

void parseString(char* string, int* nbHost) {
    char* pair = NULL;
    char* key;
    int count = 0;
    pair = strtok(string, "&");
    while (pair != NULL) {
        switch (count % 5) {
            case 0:
                hosts[count / 5] = (pair + 3);
                if (strlen(pair + 3) > 0) (*nbHost)++;
                break;
            case 1:
                ports[count / 5] = (pair + 3);
                break;
            case 2:
                files[count / 5] = (pair + 3);
                break;
            case 3:
                socksip[count / 5] = (pair + 4);
                break;
            case 4:
                socksport[count / 5] = (pair + 4);
                break;
        }
        ++count;
        pair = strtok(NULL, "&");
    }
}
int printMsg(const int i, char* string) {
    char* tmp = string;
    char buf[2048];
    memset(buf, 0, 2048);
    int c = 0;
    while (*string != '\0') {
        if (*string == '%' && *(string + 1) == ' ') {
            return 1;
        }
        if (*string == '\n') {
            buf[c] = '\0';
            printf(SCRIPT_MESSAGE, mlist[i], buf);
            fflush(stdout);
            memset(buf, 0, 2048);
            c = 0;
            ++string;
            continue;
        } else if (*string == '"') {
            buf[c] = '&';
            buf[++c] = 'q';
            buf[++c] = 'u';
            buf[++c] = 'o';
            buf[++c] = 't';
            buf[++c] = ';';
        } else if (*string == '<') {
            buf[c] = '&';
            buf[++c] = 'l';
            buf[++c] = 't';
            buf[++c] = ';';
        } else if (*string == '>') {
            buf[c] = '&';
            buf[++c] = 'g';
            buf[++c] = 't';
            buf[++c] = ';';
        } else {
            buf[c] = *string;
        }
        ++c;
        ++string;
    }
    if (strlen(buf) > 0) {
        printf(SCRIPT_MESSAGE, mlist[i], buf);
        fflush(stdout);
    }
    return 0;
}
