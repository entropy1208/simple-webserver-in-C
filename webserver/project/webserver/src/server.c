#include<stdio.h>
#include<stdlib.h>
#include<ctype.h>
#include<sys/types.h>
#include<unistd.h>
#include<strings.h>
#include<string.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<sys/stat.h>
#include<time.h>
#include<errno.h>
#include<signal.h>

#define GET "GET"
#define HEAD "HEAD"
#define HTTP "HTTP/1.1"
#define MAX_PATH_LENGTH 500
#define MAX_FILE_NAME 150
#define WWW_DIR "../www"
#define SERVER_NAME "S1"
#define DEFAULT_PORT 1024
#define BUFSIZE 1024

// Copy a string safely
void strcpy_safe(char *dest, char *src, size_t elem)	{	
	if (strlen(src) < elem)	{
		strncpy(dest, src, strlen(src));
	}
	dest[strlen(dest)] = '\0';
}

// An error wrapper 
void error(char *msg) {
  perror(msg);
  exit(1);
}

void print_help()	{
    printf("-h : Print help text\n");
    printf("-p port : Listen to port number - port\n");
}

// Removes parameters from the url
void remove_params_from_url(char *dest, char *src, size_t elem)	{
	strcpy_safe(dest, src, elem);
	int len =  strlen(dest);
	char p_flag = '?';
	char *params = strchr(dest, p_flag);
	if (params != NULL)	{
		int params_len = strlen(params);
		dest[len - params_len] = '\0';
	}
}

// Add "index.html" if neccessary
void add_index_html(char *str)	{
	if (strcmp(str, "/") == 0 || strcmp(str, "") == 0 || strcmp(str, "/\n") == 0)	{
		strcpy_safe(str, "/index.html", MAX_FILE_NAME);
	}
}

// If the string doesn't start with a slash, it will add one
void add_slash(char	*str)	{
    if (str[0] != '/')	{
		char tmp[MAX_FILE_NAME] = "/";
        strncat(tmp, str, MAX_FILE_NAME - 1);
        strcpy_safe(str, tmp, MAX_FILE_NAME);
    }
}

// Set the default header for server response
void set_default_header(char *str, int status)	{
	char *status_msg;

	switch(status)	{
		case 200:
			status_msg = "OK!";
			break;
        case 400:
            status_msg = "Bad request!";
			break;
        case 403:
            status_msg = "Forbidden!";
			break;
        case 404:
            status_msg = "Not Found!";
			break;
        case 500:
            status_msg = "Internal Server Error!";
			break;
		case 501: 
			status_msg = "Not Implemented!";
			break;
		default:
			return;
	}
	
    time_t t = time(0);
    struct tm *gmt = gmtime(&t);
    char time_str[100];
    strftime(time_str, 100, "%a, %d %b %Y %X GMT", gmt);
    snprintf(str, 9999,	"%s %i %s \r\nDate: %s \r\nServer: %s \r\nContent-Type: text/html \r\n",
        	 HTTP, status, status_msg, time_str, SERVER_NAME);  
}

// Sends a response with code (int code) to the socket with id (int socket). Only used for errors! 
void send_error_response(int socket, int code)	{
	char msg[BUFSIZE];
    char body[500];
    char header[400];
    char content_length[100];

    set_default_header(header, code);
    snprintf(body, 500, "<!DOCTYPE html><HTML><HEAD><TITLE>Error!</TITLE></HEAD><BODY><H1>No results!</H1></BODY></HTML>");
    snprintf(content_length, 100, "Content-Length: %d \r\n", strlen(body));   
    strcpy_safe(msg, header, sizeof(msg) - 1);
    strncat(msg, content_length, sizeof(msg) - 1);
    strncat(msg, "\r\n", sizeof(msg) - 1);
    strncat(msg, body, sizeof(msg) - 1);   
	write(socket, msg, strlen(msg));
}

int handle_request(int socket)	{
	FILE *stream;
	char buff[BUFSIZE];
	char method[BUFSIZE];
  	char uri[BUFSIZE];
	char version[BUFSIZE];

    if ((stream = fdopen(socket, "r+")) == NULL)	{
      error("ERROR on fdopen");
	}
    fgets(buff, BUFSIZE, stream);
    printf("%s", buff);
    sscanf(buff, "%s %s %s\n", method, uri, version);	
	
	// Check method
	int is_get; //0=HEAD; 1=GET
	if (strcmp(method, GET) == 0)	{
		is_get = 1;
	}
	else if (strcmp(method, HEAD) == 0)	{
		is_get = 0;
    }
	else if (
        strcmp(method,"POST") == 0 ||
        strcmp(method,"PUT") == 0 ||
        strcmp(method,"DELETE") == 0 ||
        strcmp(method,"CONNECT") == 0 ||
        strcmp(method,"OPTIONS") == 0 ||
        strcmp(method,"TRACE") == 0 ||
        strcmp(method,"PATCH") == 0
        )	{
        send_error_response(socket, 501);
        return;
	}
	else	{
		send_error_response(socket, 400);
		return;
	}
	// Check if simple or full request
	int is_full = 1; //0=Simple; 1=Full
	if (version == NULL)	{
		is_full = 0;
	}
	else if (strcmp(version, HTTP) != 0)	{
        send_error_response(socket, 400);
        return;
    }

	// Check if path is too long			
    if (strlen(uri) > MAX_PATH_LENGTH)	{
        send_error_response(socket, 400);
        return;
    }
	
	// Check if path could read files outside of the www-directory	
	else if (strstr(uri, "..") != NULL)	{
        send_error_response(socket, 403);
        return;
    }

	char file_name[MAX_FILE_NAME];
	char current_directory[MAX_PATH_LENGTH];
	char real_path[MAX_PATH_LENGTH];
	char wwwPath[MAX_PATH_LENGTH];
	remove_params_from_url(file_name, uri, sizeof(file_name));
	strcpy_safe(file_name, uri, sizeof(file_name));
	add_index_html(file_name);
	add_slash(file_name);
    
	// Creating path including the pointer to www-folder
   	if (getcwd(current_directory, sizeof(current_directory)) != NULL)	{
	}
	else {
       	perror("getcurrent_directory() error");
       	return 1;
   	}
	strncpy(wwwPath, current_directory, sizeof(wwwPath) - 1);
	strncat(wwwPath, "/", sizeof(wwwPath) - 1);	
    strncat(wwwPath, WWW_DIR, sizeof(wwwPath) - 1);
    strncat(wwwPath, file_name, sizeof(wwwPath) - 1);
	if (realpath(wwwPath, real_path) == NULL)	{
        int status_code;
        switch(errno)	{
            case ENOENT:
				status_code = 404;
				break;
            case EACCES:
				status_code = 403;
				break;
            case EIO:
				status_code = 500;
				break;
            default:
				status_code = 400;
				break; 
        }
        send_error_response(socket, status_code);
		return;
	}

	// Use stat to see if file can be accessed and to get status of the file
    struct stat file_stat;
    if (stat(real_path, &file_stat) != 0)	{
        int status_code;
        switch(errno)	{
            case EACCES:
				status_code = 403;
				break;
            case ENOENT:
				status_code = 404;
				break;
            case ENOMEM:
				status_code = 500;
				break;
            default:
				status_code = 500;
				break;
        }
        send_error_response(socket, status_code);
        return;
    }
    
	// Send the header, if it's a "full" request
	char statusAndHeader[9999];
    if (is_full)	{
        char header[1000];
        char default_header[500];
        char extended_header[500];
        set_default_header(default_header, 200);
        struct tm *gmt = gmtime(&file_stat.st_mtim.tv_sec);
        char time_str[100];
        strftime(time_str, 100, "%a, %d %b %Y %X GMT", gmt);
        snprintf(extended_header, 500, "Content-Length: %li\r\n"
            	 "Last-Modified: %s\r\n", file_stat.st_size, time_str);
        strcpy_safe(header, default_header, sizeof(header) - 1);
        strncat(header, extended_header, sizeof(header) - 1);
        strncat(header, "\r\n", sizeof(header) - 1);
        write(socket, header, strlen(header));
    }
	
    // Send the file as body, if the method is GET 
    if (is_get)	{
        FILE *file;
        if	((file = fopen(real_path, "r")) == NULL)	{
        	send_error_response(socket, 500);
            return;
        }
        int stream = fileno(file);
        int bytes; 
        char buff[1024]; 
        while((bytes = read(stream, buff, 1024)) > 0){
            write(socket, buff, bytes);
        }
        fclose(file);
    }
}

int main(int argc, char* argv[])	{
    int port = DEFAULT_PORT;

	/* check command line args */
	if (argc == 1)	{
		port = DEFAULT_PORT;
	}
	else if (argc == 2 || argc == 3)	{
		if (argc == 2)	{
			if (strcmp(argv[1], "-h") == 0)	{
				print_help();
				exit(0);
			}
			else	{
				print_help();
				exit(3);
			}
		}
		else	{
			if (strcmp(argv[1], "-p") == 0)	{
				char *c = argv[2];
                while(*c)	{
                    if (isdigit(*c++) == 0)	{
                        print_help();
                        exit(3);
					}
                };
				port = atoi(argv[2]);
			}
			else	{
				print_help();
				exit(3);
			}
		}
	}
	else	{
		print_help();
		exit(3);
	}
	
	/* open socket descriptor */
	int socket_id = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_id < 0)	{
		printf("Could not open socket!");
		exit(1);
	}
	
	/* bind port to socket */
	struct sockaddr_in server_address;
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(port);
	server_address.sin_addr.s_addr = INADDR_ANY;
	if (bind(socket_id, (struct sockaddr *) & server_address, sizeof(server_address)) != 0)	{
        switch (errno)	{
        	case EACCES:
				printf("Permission denied! Try to run as root\n");
				break;
            case EADDRINUSE:
				printf("Port or address already in use!\n");
				break;
            default:
				printf("Bind failed!");
        }
        exit(1);
    }
	
	/* get us ready to accept connection requests */
	listen(socket_id, 5);
    printf("Server started at port %i\n", port);

   /* 
   	* main loop: wait for a connection request, parse HTTP,
   	* serve requested content, close connection.
   	*/
	struct sockaddr_in client_address;
	int sizeof_client_address = sizeof(client_address);
	while (1)	{
		int newsocket_id = accept(socket_id, (struct sockaddr *) &client_address, &sizeof_client_address);
		if (newsocket_id < 0)	{
      		error("ERROR on accept!");
		}
		int pid = fork();
		if (pid < 0)	{
			perror("ERROR in fork!");
			exit(1);
      	}
		if (pid == 0)	{
			close(socket_id);
			handle_request(newsocket_id);
			exit(0);
		}
		else	{
			signal(SIGCHLD,SIG_IGN);
			close(newsocket_id);
		}
	} 
    exit(0);
}
