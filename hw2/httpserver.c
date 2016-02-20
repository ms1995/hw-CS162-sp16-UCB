#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <unistd.h>

#include "libhttp.h"

/*
 * Global configuration variables.
 * You need to use these in your implementation of handle_files_request and
 * handle_proxy_request. Their values are set up in main() using the
 * command line arguments (already implemented for you).
 */
int server_port;
char *server_files_directory;
char *server_proxy_hostname;
int server_proxy_port;

int file_exists(char *filename) {
  struct stat buffer;   
  if (stat(filename, &buffer))
  	return 0;
  return buffer.st_size;
}

void send_not_found(int fd) {
	http_start_response(fd, 404);
	http_send_header(fd, "Content-type", "text/html");
	http_end_headers(fd);
	http_send_string(fd,
	    "<center>"
	    "<h1>404 Not Found</h1>"
	    "<hr>"
	    "<p>WTF</p>"
	    "</center>");
}

void send_bad_request(int fd) {
	http_start_response(fd, 400);
	http_send_header(fd, "Content-type", "text/html");
	http_end_headers(fd);
	http_send_string(fd,
	    "<center>"
	    "<h1>400 Bad Request</h1>"
	    "<hr>"
	    "<p>WTF</p>"
	    "</center>");
}

void send_file(char *filename, int fd) {
	int fsize = file_exists(filename);
	if (!fsize) {
		send_not_found(fd);
		return;
	}
	size_t bytes_read;
	char buff[255];
	FILE *fp = fopen(filename, "rb");
	http_start_response(fd, 200);
	http_send_header(fd, "Content-type", http_get_mime_type(filename));
	sprintf(buff, "%d", fsize);
	http_send_header(fd, "Content-Length", buff);
	http_end_headers(fd);
	while (bytes_read = fread(buff, 1, sizeof(buff), fp))
		http_send_data(fd, buff, bytes_read);
	fclose(fp);
}

void show_dir_content(char *dir, char *uri, int fd) {
	int buf_size = 1024, used_size, uri_size = strlen(uri);
	char *buffer = malloc(buf_size);
	sprintf(buffer, "<h1>Index of %s</h1><a href=\"../\">Parent directory</a><br><br>", dir);
	used_size = strlen(buffer);
	struct dirent *dp;
	DIR *dirp = opendir(dir);
	if (dirp) {
		errno = 0;
		while (dp = readdir(dirp)) {
			if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
				continue;
			int html_len = strlen(dp->d_name) * 2 + uri_size + 21;
	    while (used_size + html_len >= buf_size) {
	    	buf_size *= 2;
		    char *new_buffer = malloc(buf_size);
		    strcpy(new_buffer, buffer);
		    free(buffer);
		    buffer = new_buffer;
	  	}
	  	sprintf(buffer + used_size, "<a href=\"%s/%s\">%s</a><br>", uri, dp->d_name, dp->d_name);
	  	used_size = strlen(buffer);
		}
		closedir(dirp);
	} else {
	  free(buffer);
	  send_not_found(fd);
	  return;
	}

	char buff[255];
	http_start_response(fd, 200);
	http_send_header(fd, "Content-type", "text/html");
	sprintf(buff, "%d", used_size);
	http_send_header(fd, "Content-Length", buff);
	http_end_headers(fd);
	http_send_string(fd, buffer);

	free(buffer);
}

/*
 * Reads an HTTP request from stream (fd), and writes an HTTP response
 * containing:
 *
 *   1) If user requested an existing file, respond with the file
 *   2) If user requested a directory and index.html exists in the directory,
 *      send the index.html file.
 *   3) If user requested a directory and index.html doesn't exist, send a list
 *      of files in the directory with links to each.
 *   4) Send a 404 Not Found response.
 */
void handle_files_request(int fd) {

  /* YOUR CODE HERE (Feel free to delete/modify the existing code below) */

  struct http_request *request = http_request_parse(fd);

  if (request == NULL || request->path == NULL || request->path[0] != '/') {
	  send_bad_request(fd);
	  return;
  }

  int dirpath_length = strlen(server_files_directory);
  // if (server_files_directory[dirpath_length - 1] == '/')
  // 	--dirpath_length;
  // char *filepath = malloc(dirpath_length + strlen(request.path) + 1);
  char *filepath = malloc(dirpath_length + strlen(request->path) + 1);
  strcpy(filepath, server_files_directory);
  strcpy(filepath + dirpath_length, request->path);
  dirpath_length = strlen(filepath);

  struct stat buffer;
  if (stat(filepath, &buffer)) {
  	send_not_found(fd);
  	free(filepath);
  	return;
  }

  if (S_ISDIR(buffer.st_mode)) {
  	char *indexpath = malloc(dirpath_length + 12); // "/index.html", length = 11
  	strcpy(indexpath, filepath);
  	strcpy(indexpath + dirpath_length, "/index.html");
  	if (file_exists(indexpath)) {
  		send_file(indexpath, fd);
  	} else {
  		show_dir_content(filepath, request->path, fd);
  	}
  	free(indexpath);
  } else {
  	send_file(filepath, fd);
  }

  free(filepath);
}

/*
 * Opens a connection to the proxy target (hostname=server_proxy_hostname and
 * port=server_proxy_port) and relays traffic to/from the stream fd and the
 * proxy target. HTTP requests from the client (fd) should be sent to the
 * proxy target, and HTTP responses from the proxy target should be sent to
 * the client (fd).
 *
 *   +--------+     +------------+     +--------------+
 *   | client | <-> | httpserver | <-> | proxy target |
 *   +--------+     +------------+     +--------------+
 */
void handle_proxy_request(int fd) {

  /* YOUR CODE HERE */

}

/*
 * Opens a TCP stream socket on all interfaces with port number PORTNO. Saves
 * the fd number of the server socket in *socket_number. For each accepted
 * connection, calls request_handler with the accepted fd number.
 */
void serve_forever(int *socket_number, void (*request_handler)(int)) {

  struct sockaddr_in server_address, client_address;
  size_t client_address_length = sizeof(client_address);
  int client_socket_number;
  pid_t pid;

  *socket_number = socket(PF_INET, SOCK_STREAM, 0);
  if (*socket_number == -1) {
    perror("Failed to create a new socket");
    exit(errno);
  }

  int socket_option = 1;
  if (setsockopt(*socket_number, SOL_SOCKET, SO_REUSEADDR, &socket_option,
        sizeof(socket_option)) == -1) {
    perror("Failed to set socket options");
    exit(errno);
  }

  memset(&server_address, 0, sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = INADDR_ANY;
  server_address.sin_port = htons(server_port);

  if (bind(*socket_number, (struct sockaddr *) &server_address,
        sizeof(server_address)) == -1) {
    perror("Failed to bind on socket");
    exit(errno);
  }

  if (listen(*socket_number, 1024) == -1) {
    perror("Failed to listen on socket");
    exit(errno);
  }

  printf("Listening on port %d...\n", server_port);

  while (1) {

    client_socket_number = accept(*socket_number,
        (struct sockaddr *) &client_address,
        (socklen_t *) &client_address_length);
    if (client_socket_number < 0) {
      perror("Error accepting socket");
      continue;
    }

    printf("Accepted connection from %s on port %d\n",
        inet_ntoa(client_address.sin_addr),
        client_address.sin_port);

    pid = fork();
    if (pid > 0) {
      close(client_socket_number);
    } else if (pid == 0) {
      // Un-register signal handler (only parent should have it)
      signal(SIGINT, SIG_DFL);
      close(*socket_number);
      request_handler(client_socket_number);
      close(client_socket_number);
      exit(EXIT_SUCCESS);
    } else {
      perror("Failed to fork child");
      exit(errno);
    }
  }

  close(*socket_number);

}

int server_fd;
void signal_callback_handler(int signum) {
  printf("Caught signal %d: %s\n", signum, strsignal(signum));
  printf("Closing socket %d\n", server_fd);
  if (close(server_fd) < 0) perror("Failed to close server_fd (ignoring)\n");
  exit(0);
}

char *USAGE =
  "Usage: ./httpserver --files www_directory/ --port 8000\n"
  "       ./httpserver --proxy inst.eecs.berkeley.edu:80 --port 8000\n";

void exit_with_usage() {
  fprintf(stderr, "%s", USAGE);
  exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
  signal(SIGINT, signal_callback_handler);

  /* Default settings */
  server_port = 8000;
  void (*request_handler)(int) = NULL;

  int i;
  for (i = 1; i < argc; i++) {
    if (strcmp("--files", argv[i]) == 0) {
      request_handler = handle_files_request;
      free(server_files_directory);
      server_files_directory = argv[++i];
      if (!server_files_directory) {
        fprintf(stderr, "Expected argument after --files\n");
        exit_with_usage();
      }
    } else if (strcmp("--proxy", argv[i]) == 0) {
      request_handler = handle_proxy_request;

      char *proxy_target = argv[++i];
      if (!proxy_target) {
        fprintf(stderr, "Expected argument after --proxy\n");
        exit_with_usage();
      }

      char *colon_pointer = strchr(proxy_target, ':');
      if (colon_pointer != NULL) {
        *colon_pointer = '\0';
        server_proxy_hostname = proxy_target;
        server_proxy_port = atoi(colon_pointer + 1);
      } else {
        server_proxy_hostname = proxy_target;
        server_proxy_port = 80;
      }
    } else if (strcmp("--port", argv[i]) == 0) {
      char *server_port_string = argv[++i];
      if (!server_port_string) {
        fprintf(stderr, "Expected argument after --port\n");
        exit_with_usage();
      }
      server_port = atoi(server_port_string);
    } else if (strcmp("--help", argv[i]) == 0) {
      exit_with_usage();
    } else {
      fprintf(stderr, "Unrecognized option: %s\n", argv[i]);
      exit_with_usage();
    }
  }

  if (server_files_directory == NULL && server_proxy_hostname == NULL) {
    fprintf(stderr, "Please specify either \"--files [DIRECTORY]\" or \n"
                    "                      \"--proxy [HOSTNAME:PORT]\"\n");
    exit_with_usage();
  }

  serve_forever(&server_fd, request_handler);

  return EXIT_SUCCESS;
}
