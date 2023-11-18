/*
    Abhinav Khanna
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define ERROR 1
#define REQUIRED_ARGC 3
#define PROTOCOL "tcp"
#define BUFLEN 1024
#define ALLOWED_INVALID_CMDS 4 // the four invalid commands are the project filename, port number, document document_dir, auth token
#define QLEN 1
#define EXTRABUF 256
#define URL_PLACES_TO_COMPARE 5
#define FIRST 1
#define METHOD_INDEX 0
#define FILE_INDEX 1
#define PROTOCOL_INDEX 2

bool nOption, dOption, aOption, isRequestValid = false;
char *port;
char *document_dir;
char *auth_token;
char *header_parts[EXTRABUF];
char http_method[EXTRABUF], argument[EXTRABUF], path[EXTRABUF], protocol_var[EXTRABUF];

int usage(char *progname)
{
    fprintf(stderr, "usage: %s host port\n", progname);
    exit(ERROR);
}

int errexit(char *format, char *arg)
{
    fprintf(stderr, format, arg);
    fprintf(stderr, "\n");
    exit(ERROR);
}

// helper Method to check if the line ends with \r\n
bool endsWithCRLF(const char *str)
{
    size_t len = strlen(str);
    return len >= 2 && str[len - 2] == '\r' && str[len - 1] == '\n';
}

// Method to parse the request and get the parts of the header
void parseRequest(char *string)
{
    isRequestValid = false;

    char *ptr = malloc(EXTRABUF);

    ptr = strtok(string, " ");
    int index = 0;

    while (ptr != NULL)
    {
        header_parts[index] = ptr;
        ptr = strtok(NULL, " ");
        index++;
    }

    // Check if the line ends with a "\r\n"
    if (index == 3 && endsWithCRLF(header_parts[2]))
    {
        isRequestValid = true;
    }
}

// Check if the header contains a valid protocol
int checkValidProtocol(int sd2, char *protocol)
{
    if (strncmp(protocol, "HTTP/", URL_PLACES_TO_COMPARE) != 0)
    {
        char *toReturn = "HTTP/1.1 501 Protocol Not Implemented\r\n\r\n";
        if (write(sd2, toReturn, strlen(toReturn)) < 0)
        {
            errexit("Error writing message to server", toReturn);
        }
        close(sd2);
        return 1;
    }
    return 0;
}

void startListening(int sd)
{
    struct sockaddr addr;
    unsigned int addrlen;
    int sd2, size, index = 0;
    char *buffer = malloc(BUFLEN);
    char *toReturn = malloc(BUFLEN);

    // listens for a connection
    if (listen(sd, QLEN) < 0)
        errexit("ERROR: unable to listen on port %s\n", port);

    /* accept a connection */
    addrlen = sizeof(addr);
    sd2 = accept(sd, &addr, &addrlen);
    if (sd2 < 0)
    {
        errexit("ERROR: error in accepting connection from socket", NULL);
    }

    // open the connection
    FILE *sp = fdopen(sd2, "rb");
    if (sp < 0)
    {
        errexit("ERROR: unable to open toReturn", NULL);
    }

    if (fgets(buffer, BUFLEN, sp) < 0)
    {
        errexit("ERROR: error in reading file: %s", NULL);
    }

    parseRequest(buffer);
    strcpy(http_method, header_parts[METHOD_INDEX]);
    strcpy(argument, header_parts[FILE_INDEX]);
    strcpy(protocol_var, header_parts[PROTOCOL_INDEX]);

    if (isRequestValid == false)
    {
        toReturn = "HTTP/1.1 400 Malformed Request\r\n\r\n";
        if (write(sd2, toReturn, strlen(toReturn)) < 0)
        {
            errexit("ERROR: error in writing message: %s", toReturn);
        }
        close(sd2);
        startListening(sd);
    }
    else
    {
        buffer[0] = '\0';
        while (1)
        {
            if (fgets(buffer, BUFLEN, sp) < 0)
            {
                errexit("ERROR: error in reading the file: %s", NULL);
            }
            if (strcmp(buffer, "\r\n") == 0)
            {
                break;
            }
            if (buffer[strlen(buffer) - 2] != '\r' || buffer[strlen(buffer) - 1] != '\n')
            {
                toReturn = "HTTP/1.1 400 Malformed Request\r\n\r\n";
                if (write(sd2, toReturn, strlen(toReturn)) < 0)
                {
                    errexit("ERROR: error in writing message: %s", toReturn);
                }
                close(sd2);
                startListening(sd);
            }
        }

        if (checkValidProtocol(sd2, protocol_var) == 1)
        {
            startListening(sd);
        }

        if (!strcmp(http_method, "GET"))
        {
            if (argument[0] != '/')
            {
                char *str = "HTTP/1.1 406 Invalid Filename\r\n\r\n";
                if (write(sd2, str, strlen(str)) < 0)
                {
                    errexit("ERROR: error in writing message: %s", str);
                }
                close(sd2);
                startListening(sd);
            }
            else
            {
                if (!strcmp(argument, "/"))
                {
                    strcpy(argument, "/homepage.html");
                }
                strcpy(path, document_dir);
                strcat(path, argument);

                if (access(path, F_OK) == 0)
                {
                    FILE *fp = fopen(path, "rb");
                    if (fp < 0)
                    {
                        errexit("ERROR: unable to open file %s\n", NULL);
                    }
                    toReturn = "HTTP/1.1 200 OK\r\n\r\n";
                    if (write(sd2, toReturn, strlen(toReturn)) < 0)
                    {
                        errexit("ERROR: error in writing message: %s", toReturn);
                    }
                    while ((size = fread(buffer, 1, sizeof(buffer), fp)) > 0)
                    {
                        if (write(sd2, buffer, size) < 0)
                        {
                            errexit("ERROR: error in writing message", NULL);
                            break; // Stop if there's an error
                        }
                        memset(buffer, 0x0, BUFLEN);
                    }
                    memset(buffer, 0x0, size);
                    fclose(fp);
                    close(sd2);
                    startListening(sd);
                }
                else
                {
                    toReturn = "HTTP/1.1 404 File Not Found\r\n\r\n";
                    if (write(sd2, toReturn, strlen(toReturn)) < 0)
                    {
                        errexit("ERROR: error in writing message: %s", toReturn);
                    }
                    close(sd2);
                    startListening(sd);
                }
            }
        }
        else if (!strcmp(http_method, "SHUTDOWN"))
        {
            if (!strcmp(argument, auth_token))
            {
                toReturn = "HTTP/1.1 200 Server Shutting Down\r\n\r\n";
                if (write(sd2, toReturn, strlen(toReturn)) < 0)
                {
                    errexit("ERROR: error in writing message: %s", toReturn);
                }
                close(sd);
                close(sd2);
                exit(0);
            }
            else
            {
                toReturn = "HTTP/1.1 403 Operation Forbidden\r\n\r\n";
                if (write(sd2, toReturn, strlen(toReturn)) < 0)
                {
                    errexit("ERROR: error in writing message: %s", toReturn);
                }
                close(sd2);
                startListening(sd);
            }
        }
        else
        {
            toReturn = "HTTP/1.1 405 Unsupported Method\r\n\r\n";
            if (write(sd2, toReturn, strlen(toReturn)) < 0)
            {
                errexit("ERROR: error in writing message %s", toReturn);
            }
            close(sd2);
            startListening(sd);
        }
    }
    close(sd2);
}

void checkPortNumber(int number_port)
{
    if (number_port < 1025 || number_port > 65535)
    {
        errexit("Invalid port number. Please enter a port number between [1025, 65535]", NULL);
    }
}

/* Method to create a socket and bind */
void socketWork()
{
    struct sockaddr_in sin;
    struct sockaddr addr;
    struct protoent *protoinfo;
    unsigned int addrlen;
    int sd, sd2;

    /* determine protocol */
    if ((protoinfo = getprotobyname(PROTOCOL)) == NULL)
    {
        errexit("cannot find protocol information for %s", PROTOCOL);
    }

    /* setup endpoint info */
    memset((char *)&sin, 0x0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons((u_short)atoi(port));

    /* allocate a socket */
    sd = socket(PF_INET, SOCK_STREAM, protoinfo->p_proto);
    if (sd < 0)
    {
        errexit("cannot create socket", NULL);
    }

    /* bind the socket */
    if (bind(sd, (struct sockaddr *)&sin, sizeof(sin)) < 0)
    {
        errexit("cannot bind to port %s", port);
    }

    /* listen for incoming connections */
    if (listen(sd, QLEN) < 0)
    {
        errexit("cannot listen on port %s\n", port);
    }

    startListening(sd);
    /* close connections and exit */
    close(sd);
    exit(0);
}

/* Main method which basically checks the arguments and updates the flags for the commands*/
int main(int argc, char *argv[])
{
    if (argc <= FIRST) // arguments are less than equal to 1. Basically invalid number of arguments
    {
        errexit("ERROR: Invalid Number of arguments. You must give atleast -n, -d, -a and their arguments. \n", NULL);
        return 1;
    }
    else
    {
        int invalid = 0;
        // storing the index of the url for convenience
        int portIndex = -1;
        int authIndex = -1;
        // Check -n -d -a and update the flags
        for (int index = 0; index < argc; index++)
        {
            if (!strcmp(argv[index], "-n"))
            {
                nOption = true;
                port = argv[index + 1];
            }
            else if (!strcmp(argv[index], "-d"))
            {
                dOption = true;
                document_dir = argv[index + 1];
            }
            else if (!strcmp(argv[index], "-a"))
            {
                aOption = true;
                authIndex = index + 1;
                auth_token = argv[index + 1];
            }
            else
            {
                invalid++;
            }
        }

        if (invalid > ALLOWED_INVALID_CMDS)
        {
            errexit("Error. Invalid Arguments", NULL);
        }

        if (!nOption)
        {
            errexit(" -n and it's arguments must be present", NULL);
        }

        if (!dOption)
        {
            errexit(" -d and it's arguments must be present", NULL);
        }

        if (!aOption)
        {
            errexit(" -a and it's arguments must be present", NULL);
        }
        checkPortNumber(atoi(port));
        socketWork();
    }
}
