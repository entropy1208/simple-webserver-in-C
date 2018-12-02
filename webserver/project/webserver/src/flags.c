#include<stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void printhelp(){
    printf("-h Print help text.\n");
    printf("-p port Listen to port number port\n");
    printf("-d Run as a daemon instead of as a normal program.\n");
    printf("-l logfile Log to logfile. If this option is not specified, logging will be output to syslog, which is the default.\n");
    printf("-s [fork | thread | prefork | mux] Select request handling method.\n");
}

enum handlingMethod {
    fork,
    thread,
    prefork,
    mux
};

int main(int argc, char* argv[])
{
    int port = 80;
    int run_as_daemon = 0;
    char *logfile;
    enum handlingMethod handling_method = fork;
    
    
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
        }
        //DAEMON
        else if (strcmp(argv[i],"-d")==0){
            run_as_daemon = 1;
        }
        //LOGFILE   
        else if (strcmp(argv[i],"-l")==0){
            i++;
            if (!(i < argc)){
                printhelp();
                exit(3);
            }
            logfile = argv[i];
        }
        //HANDLINGMETHOD
        else if (strcmp(argv[i],"-s")==0){
            i++;
            if (!(i < argc)){
                printhelp();
                exit(3);
            } else if (strcmp(argv[i],"fork")==0){
                handling_method = fork;
            } else if (strcmp(argv[i],"thread")==0){
                handling_method = thread;
            } else if (strcmp(argv[i],"prefork")==0){
                handling_method = prefork;
            } else if ( strcmp(argv[i],"mux")==0){
                handling_method = mux;
            } else {
                printhelp();
                exit(3);
            }
         
            
        }
        i++;
    }
    
    printf("Port %d\n", port);
    printf("Daeomn %d\n", run_as_daemon);
    printf("Logfile %s\n", logfile);
    printf("Handling method %d\n", handling_method);

    
    exit(0);
}


