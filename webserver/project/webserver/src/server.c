#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h> 
#include <unistd.h> 
#include <strings.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <signal.h>

#define GET "GET"
#define HEAD "HEAD"
#define HTTP1 "HTTP/1.0"
#define HTTP11 "HTTP/1.1"
#define MAX_PATH_LENGTH 150
#define WWW_DIR "../www"
#define SERVER_NAME "A2_BeniAndKush"
#define DEFAULT_PORT 80

void printhelp(){
    printf("-h Print help text.\n");
    printf("-p port Listen to port number port\n");
    printf("-d Run as a daemon instead of as a normal program. [NOT SUPPORTED]\n");
    printf("-l logfile Log to logfile. If this option is not specified, logging will be output to syslog, which is the default. [NOT SUPPORTED]\n");
    printf("-s [fork | thread | prefork | mux] Select request handling method. [NOT SUPPORTED]\n");
}


//Removes paramter from url
void removeParam(char* url, char* str){
	strcpy(str, url);
	int len =  strlen(str);
	char p_flag = '?';
	char *param = strchr(str, p_flag);
	if(param!=NULL){	
		int param_len = strlen(param);
		str[len-param_len] = '\0';
	}
}

void setDefaultHeader(char *str, int status){
    time_t t = time(0);
    struct tm *gmt = gmtime(&t);
    char timeString[100];
    strftime(timeString, 100, "%a, %d %b %Y %X GMT", gmt);
    
    snprintf(str, 9999,
        "%s %i\r\n"
        "Content-Type: text/html\r\n"
        "Server: %s\r\n"
        "Date: %s\r\n",
        HTTP1,
        status,
        SERVER_NAME,
        timeString);  
}

//Adds "index.html" if neccessary
void addIndexHtml(char* str){
	if(strcmp(str,"/") == 0 || strcmp(str,"") == 0 || strcmp(str,"/\n") == 0){
		strcpy(str,"/index.html");
	}
}

//If the string doesnt start with a slash, it will add one
void addSlash(char* str){
    if(str[0] != '/'){
        char tmp[MAX_PATH_LENGTH+1];
        strcpy(tmp, "/");
        strcat(tmp, str);
        strcpy(str, tmp);
    }
}

//Sends a respnse with code (int code) to the socket with id (int socket). Only used for errors
void statusResponse(int socket, int code){
    char *status_message;
	switch(code) {
        case 400:
            status_message = "Bad request"; break;
        case 403:
            status_message = "Forbidden"; break;
        case 404:
            status_message = "Not Found"; break;
        case 500:
            status_message = "Internal Server Error"; break;
		case 501: 
			status_message = "Not Implemented"; break;
		default:
			return;
	}
    
    char message[1000];
    char body[500];
    char header[400];
    char content_length[100];
    
    setDefaultHeader(header, code);
    snprintf(body, 500,
        "<!DOCTYPE html><HTML><HEAD><TITLE>%i %s</TITLE></HEAD><BODY><H1>%i %s</H1></BODY></HTML>",
        code, status_message, code, status_message);
    
    snprintf(content_length, 100,
        "Content-Length: %li\r\n",
        strlen(body));

    
    strcpy(message, header);
    strcat(message, content_length);
    strcat(message, "\r\n");
    strcat(message, body);
    
	write(socket, message, strlen(message));
}

void handleRequest(int socket){
	char  buffer[9999];
	if(read(socket, buffer, 9999)<0){
        statusResponse(socket, 500);
        return;
    }
	char *method = strtok(buffer,  " \t\r\n");
	char *url = strtok(NULL, " \t");
	char *protocol = strtok(NULL, " \t\r\n"); 
    
		
    //Check if path is too long
    if(strlen(url)>MAX_PATH_LENGTH){
        statusResponse(socket, 400);
        return;
    //Check if path could read files outside of the www-directory
    }else if(strstr(url,"..") != NULL){
        statusResponse(socket, 403);
        return;
    }

	//Check method
	int isGet; //0=HEAD; 1=GET
	if(strcmp(method,GET) == 0){
		isGet=1;
	}else if(strcmp(method,HEAD) == 0){
		isGet=0;
    }else if(
        strcmp(method,"POST") == 0 ||
        strcmp(method,"PUT") == 0 ||
        strcmp(method,"DELETE") == 0 ||
        strcmp(method,"CONNECT") == 0 ||
        strcmp(method,"OPTIONS") == 0 ||
        strcmp(method,"TRACE") == 0 ||
        strcmp(method,"PATCH") == 0
        ){
        statusResponse(socket, 501);
        return;
	}else{
		statusResponse(socket, 400);
		return;
	}

	//Check if simple or full request
	int isFull = 1; //0=Simple; 1=Full
	if(protocol==NULL){
		isFull=0;
    //Check for http version (Adding HTTP/1.1 to support browsers)
	}else if(strcmp(protocol, HTTP1) != 0 && strcmp(protocol, HTTP11) != 0){
        statusResponse(socket, 400);
        return;
    }
    
	char path[999];
	removeParam(url, path);
	addIndexHtml(path);
    addSlash(path);

	char real_path [MAX_PATH_LENGTH+1];
	
    //Creating path including the pointer to www-folder
    char wwwPath[MAX_PATH_LENGTH + strlen(WWW_DIR)];
    strcpy(wwwPath, WWW_DIR);
    strcat(wwwPath, path);
    
	if(realpath(wwwPath, real_path) == NULL){
        int statusCode;
        switch(errno){
            case ENOENT: statusCode = 404;break;
            case EACCES: statusCode = 403;break;
            case EIO: statusCode = 500;break;
            default: statusCode = 400;break; 
        }
        statusResponse(socket, statusCode);
		return;
	}
    
    //Use stat to see if file can be accessed AND to get status of the file (which will be used in header)
    struct stat file_stat;
    if(stat(real_path, &file_stat) != 0){
        int statusCode;
        switch(errno){
            case EACCES: statusCode = 403;break;
            case ENOENT: statusCode = 404;break;
            case ENOMEM: statusCode = 500;break;
            default: statusCode = 500;break;
        }
        statusResponse(socket, statusCode);
        return;
    }
    
    
    char statusAndHeader[9999];
    
    //Send the header, if it's a "full"-request
    if(isFull){
        char header[1000];
        char defaultHeader[500];
        char extendedHeader[500];
        setDefaultHeader(defaultHeader, 200);

        struct tm *gmt = gmtime(&file_stat.st_mtim.tv_sec);
        char timeString[100];
        strftime(timeString, 100, "%a, %d %b %Y %X GMT", gmt);
        
        snprintf(extendedHeader, 500,
            "Content-Length: %li\r\n"
            "Last-Modified: %s\r\n",
            file_stat.st_size,
            timeString);
        
        strcpy(header, defaultHeader);
        strcat(header, extendedHeader);
        strcat(header, "\r\n");

        write(socket, header, strlen(header));
    }
    
    //Send the file as body, if the method is GET 
    if(isGet){
        FILE *file;
        if((file = fopen(real_path, "r")) == NULL){
            statusResponse(socket, 500);
            return;
        }
        
        int stream = fileno(file);
        int bytes; 
        char buffer[1024]; 
        while((bytes = read(stream, buffer, 1024)) > 0){
            write(socket, buffer, bytes);
        }
        fclose(file);
    }
}

int main(int argc, char* argv[])
{
    int port = DEFAULT_PORT;
    
    int i=1;
    while(i < argc){
        //HELP
        if (strcmp(argv[i],"-h")==0){
            printhelp();
            exit(0);
        //PORT
        }else if (strcmp(argv[i],"-p")==0){
            i++;
            if (i < argc){
                char* c = argv[i];
                //This loop checks if there is a non-numeric character in the port-flag
                while (*c) {
                    if (isdigit(*c++)==0){
                        printhelp();
                        exit(3);
                    };
                }
                port = atoi (argv[i]);
            }else{
                printhelp();
                exit(3);
            }
        }else{
            printhelp();
            exit(3);
        }
        i++;
    }

    //AF_INET = IPV4; SOCK_STREAM = TCP
	int socket_id = socket(AF_INET, SOCK_STREAM, 0);

	if(socket_id < 0){
		printf("Could not open socket");
		exit(1);
	}

	struct sockaddr_in server_address;
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(port);
	server_address.sin_addr.s_addr = INADDR_ANY; //Bind to all interfaces, not just localhost
	if(bind(socket_id, (struct sockaddr *) & server_address, sizeof(server_address)) != 0){
        switch(errno){
            case EACCES: printf("No permission. Try to run as root\n");break;
            case EADDRINUSE: printf("Port or address already in use\n");break;
            default: printf("Bind failed");
        }
        exit(1);
    }
	listen(socket_id, 5);
    printf("Started server at port %i\n", port);
       

	while(1){
		struct sockaddr_in client_address;
		int sizeof_client_address = sizeof(client_address); //Adding to variable so cast can easily be done on accept()
		int newsocket_id = accept(socket_id, (struct sockaddr *) &client_address, &sizeof_client_address);

        //Fork here -> The parent-process will continue the loop, the child-process will handle the request
		int pid = fork();
		if(pid == 0){
			close(socket_id);
			handleRequest(newsocket_id);
			exit(0);
		}else{
			signal(SIGCHLD,SIG_IGN);
			close(newsocket_id);
		}

	}
    
    exit(0);
}
