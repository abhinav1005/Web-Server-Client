/**
 * Simple command line based web clients
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
#define HOST_POS 1
#define PORT_POS 2
#define PROTOCOL "tcp"
#define BUFLEN 1024
#define PORT "80"
#define ALLOWED_INVALID_CMDS 3 // the three invalid commands are the project filename, url and the local_filename
#define EXTRA_BUF 20
#define URL_PLACES_TO_COMPARE 7
#define FIRST 1
#define SECOND 2

bool uOption, dOption, qOption, rOption, oOption, fOption = false;
char *filename;
char *hostname;
char *local_filename;
char http_request[BUFLEN];
char *hostname;
char response_header[BUFLEN];

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

void socket_work()
{

    // define outside, only if it still works
    struct sockaddr_in sin;
    struct hostent *hinfo;
    struct protoent *protoinfo;
    char buffer[BUFLEN];
    int sd, wr, ret;
    char *header = malloc(BUFLEN + 1);
    int headerFinished = 0;

    /* lookup the hostname */
    hinfo = gethostbyname(hostname);
    if (hinfo == NULL)
        errexit("cannot find name: %s", hostname);

    /* set endpoint information */
    memset((char *)&sin, 0x0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(atoi(PORT));
    memcpy((char *)&sin.sin_addr, hinfo->h_addr, hinfo->h_length);

    if ((protoinfo = getprotobyname(PROTOCOL)) == NULL)
        errexit("cannot find protocol information for %s", PROTOCOL);

    /* allocate a socket */
    sd = socket(PF_INET, SOCK_STREAM, protoinfo->p_proto);
    if (sd < 0)
        errexit("cannot create socket", NULL);

    /* connect the socket */
    if (connect(sd, (struct sockaddr *)&sin, sizeof(sin)) < 0)
        errexit("cannot connect", NULL);

    /* Format the string to the format for the request */
    snprintf(http_request, BUFLEN,
             "GET %s HTTP/1.0\r\n"
             "Host: %s\r\n"
             "User-Agent: CWRU CSDS 325 Client 1.0\r\n\r\n",
             filename, hostname);

    /* Write to the server */
    wr = write(sd, http_request, strlen(http_request));

    /*Check if writing to server was successful*/
    if (wr < 0)
    {
        errexit("Error writing to socket!", NULL);
    }

    /* I get the first header line and check if it is valid and not null*/
    FILE *file_output = fdopen(sd, "rb");
    char *first_header_line = fgets(header, BUFLEN, file_output);
    // printf("Header : %s", first_header_line);
    if (first_header_line != NULL)
    {
        // Make a copy of the first header line to preserve it to extract the status from the header
        char *first_header_copy = strdup(first_header_line);
        if (first_header_copy != NULL)
        {
            /* Extract the status code from the copy*/
            char *status = strtok(first_header_copy, " ");
            status = strtok(NULL, " ");
            /*If status is not null and it is successful, 200*/
            if (status != NULL && !strcmp(status, "200"))
            {
                /* parse till the end of the header response*/
                while (strcmp(header, "\r\n"))
                {
                    /* Print if r option is selected*/
                    if (rOption)
                    {
                        printf("INC: %s", header);
                    }
                    fgets(header, BUFLEN, file_output);
                }
                /* writing to the file */
                FILE *write_file = fopen(local_filename, "wb");
                int size = fread(header, 1, BUFLEN, file_output);

                while (size != 0)
                {
                    fwrite(header, 1, size, write_file);
                    // printf("Header: %s", header);
                    memset(header, 0x0, BUFLEN);
                    size = fread(header, 1, sizeof(header), file_output);
                }
                memset(header, 0x0, size);
                fclose(write_file);
            }
            /* Else if status is 301*/
            else if (status != NULL && !strcmp(status, "301"))
            {
                if (rOption)
                {
                    while (strcmp(header, "\r\n"))
                    {
                        printf("INC: %s", header);
                        fgets(header, BUFLEN, file_output);
                    }
                }
            }
            /* else if status is some other code*/
            else
            {
                char *header_copy = strdup(header);
                while (strcmp(header, "\r\n"))
                {
                    if (rOption)
                    {
                        printf("INC: %s", header);
                    }
                    fgets(header, BUFLEN, file_output);
                }
            }
            // Free the dynamically allocated copy of the first header line
            free(first_header_copy);
        }
    }
    else
    {
        // Handle the case where the header cannot be read
        fprintf(stderr, "Error reading the header.\n"); // use your error messsage
    }

    /* close & exit */
    close(sd);
    exit(0);
}

/* Method to check if the url contains http and check it inclusive of case sensitive options*/
int checkHTTP(char *url)
{
    char *lowercaseURL = strdup(url);
    for (int i = 0; lowercaseURL[i]; i++)
    {
        lowercaseURL[i] = tolower(lowercaseURL[i]);
    }
    int output = strncmp(lowercaseURL, "http://", URL_PLACES_TO_COMPARE) == 0;
    free(lowercaseURL);
    return output;
}

/* Method to parse the url and break it into hostname, path*/
void parseURL(char *url)
{
    if (!checkHTTP(url))
    {
        printf("Not a valid http request. We only support http requests");
        return;
    }

    char *temp = malloc(strlen(url) + EXTRA_BUF);
    char *part = strtok(url, "/");
    /* list stores the string values in the filepath*/
    char *list[EXTRA_BUF];

    int index = 0;
    while (part != NULL)
    {
        part = strtok(NULL, "/");
        list[index] = part;
        index++;
    }

    hostname = list[0];
    index = 1;
    strcat(temp, "/");

    while (list[index] != NULL)
    {
        strcat(temp, list[index]);
        if (list[index + 1] != NULL)
            strcat(temp, "/");
        index++;
    }
    if (list[FIRST] != NULL && list[SECOND] == NULL)
        strcat(temp, "/");

    filename = temp;
}

/* Helper method for -d command */
void printD()
{
    printf("DBG: host: %s\n", hostname);
    printf("DBG: web_file: %s\n", filename);
    printf("DBG: output_file: %s\n", local_filename);
}

/* Helper method for -q command */
void printQ()
{
    printf("OUT: GET %s HTTP/1.0\r\n", filename);
    printf("OUT: Host: %s\r\n", hostname);
    printf("OUT: User-Agent: CWRU CSDS 325 SimpleClient 1.0\r\n");
}

/* Main method which basically checks the arguments and updates the flags for the commands*/
int main(int argc, char *argv[])
{
    if (argc <= 1)
    {
        printf("ERROR: Invalid Number of arguments. You must give atleast -u and -o. \n");
        return 1;
    }

    if (argc >= 2)
    {
        int invalid = 0;
        // storing the index of the url for convenience
        int urlIndex = -1;
        // Check -u -d -q -r -o and update the flags
        for (int index = 0; index < argc; index++)
        {
            if (!strcmp(argv[index], "-u"))
            {
                uOption = true;
                urlIndex = index + 1;
            }
            else if (!strcmp(argv[index], "-d"))
            {
                dOption = true;
            }
            else if (!strcmp(argv[index], "-q"))
            {
                qOption = true;
            }
            else if (!strcmp(argv[index], "-r"))
            {
                rOption = true;
            }
            else if (!strcmp(argv[index], "-o"))
            {
                oOption = true;
                local_filename = argv[index + 1];
            }
            else if (!strcmp(argv[index], "-f"))
            {
                fOption = true;
            }
            else
            {
                invalid++;
            }
        }

        //
        if (invalid > ALLOWED_INVALID_CMDS)
        {
            printf("Error. Invalid Arguments");
        }
        else if (!uOption || !oOption)
        {
            printf("Error. -u and -o options are mandatory");
        }
        parseURL(argv[urlIndex]);

        if (dOption)
        {
            printD();
        }
        if (qOption)
        {
            printQ();
        }
        socket_work();
    }
}