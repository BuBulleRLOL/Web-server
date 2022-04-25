#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <signal.h>
#include "socket.h"
#include "http_parse.h"
#include "stats.h"
#define TAILLE 255

web_stats *stats;

void traitement_signal(int sig) {
	printf("Signal %d recu\n", sig);
	waitpid(-1, NULL, 0);
}

void initialiser_signaux(void) {
	if(signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
		perror("signal");
	}
	struct sigaction sa;
	sa.sa_handler = traitement_signal;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1)
	{
		perror("sigaction(SIGCHLD)");
	}
}

char *fgets_or_exit(char *buffer, int size, FILE *stream) {
	if(fgets(buffer, size, stream) == NULL) {
		exit(0);
	}
	return buffer;
}

void skip_headers(FILE *client) {
	char buffer[TAILLE];
    while(strcmp(buffer, "\r\n") != 0 && strcmp(buffer, "\n") != 0) { 
		fgets(buffer, TAILLE, client);	
	}
}

void send_status(FILE *client, int code, const char *reason_phrase) {
	fprintf(client, "HTTP/1.1 %d %s\r\n", code, reason_phrase);
}

void send_response(FILE *client, int code, const char *reason_phrase, int length, const char *message_body) {
	send_status(client, code, reason_phrase);
	fprintf(client, "Content-Length: %d\r\n\r\n%s", length, message_body);
}

char *rewrite_target(char *target) {
	if(strlen(target) < 2) {
		return "/index.html";
	}
	if(strchr(target, '?') != NULL) {
		char *string = malloc(sizeof(target)+1);
		int i = 0;
		while(target[i] != '?' && target[i] != '\0') {
			string[i] = string[i];
			i++;
		}
		i++;
		string[i] = '\0';
		return string;
	}
	return target;
}

FILE *check_and_open(const char *target, const char *document_root) {
	char *mdf_target = malloc(sizeof(target) + sizeof(document_root) + TAILLE);
	strcpy(mdf_target, document_root);
	strcat(mdf_target, target);
	struct stat st;
	if(stat(mdf_target, &st) == -1) {
		perror("stat error");
		return NULL;
	}
	if(S_ISREG(st.st_mode)) {
		FILE *f = fopen(mdf_target, "r");
		if(f == NULL) {
			perror("fopen error");
			return NULL;
		}
		printf("Fichier valide\n");
		return f;
	}
	printf("Impossible\n");
	return NULL;
}

int get_file_size(int fd) {
	struct stat st;
	if(fstat(fd, &st) < 0) {
		perror("stat error");
		return -1;
	}
	return st.st_size;
}

int copy(FILE *in, FILE *out) {
	char c;
	while(fread(&c, 1, 1, in) == 1) {
		fwrite(&c, 1, 1, out);
	}
	return 0;
}

void send_stats(FILE *client) {
	char * stat_msg = "{\n\t\"served_connections\":,\n\t\"served_requests\":,\n\t\"ok_200\":,\n\t\"ko_400\":,\n\t\"ko_403\":,\n\t\"ko_404\":\"%d\"\n};";
	send_status(client, 200, "OK");
	fprintf(client, "Content-Length: %ld\r\nContent-Type: application/json\r\n\r\n", strlen(stat_msg));
	fprintf(client, "{\n\t\"served_connections\":%d,\n\t\"served_requests\":%d,\n\t\"ok_200\":%d,\n\t\"ko_400\":%d,\n\t\"ko_403\":%d,\n\t\"ko_404\":%d\n};", stats -> served_connections, stats -> served_requests, stats -> ok_200, stats -> ko_400, stats -> ko_403, stats -> ko_404);
}

int main(int argc, char **argv) {
    init_stats();
	stats = get_stats();
    initialiser_signaux();
    if(argc > 2) {
		perror("Trop d'arguments !");
		exit(1);
	}
    DIR *repertoire;
	repertoire = opendir(argv[1]);
	if(repertoire == NULL) {
		perror("Répertoire introuvable ! ");
        exit(0);
	}
    int serveur_socket;
	printf("\033[32;01mServeur démarré sur le port 8080\033[00m\n");
	serveur_socket = creer_serveur(8080);
    if(serveur_socket == -1) {
		exit(1);
	}
	while(1) {
        int client_socket;
        client_socket = accept(serveur_socket, NULL, NULL);
        FILE *client = fdopen(client_socket, "a+");
        int tmppid;
        if(client_socket == -1) {
			perror("client socket error");
		} else {
            tmppid = fork();
			if(tmppid == -1 ) {
				perror("fork error");
			} else {
				if(tmppid == 0) {
                    stats -> served_connections++;
			        while(1) {
				        char buffer[TAILLE];
				        fgets_or_exit(buffer, TAILLE, client);
				        printf("%s", buffer);
                        http_request request;
				        int parse_ret = parse_http_request(buffer, &request);
				        if(parse_ret == -1) {
                            stats -> served_requests++;
                            stats -> ko_400++;
					        char *message = "Bad Request\r\n";
				            send_response(client, 400, "Bad Request", strlen(message), message);
				            fclose(client);
				            return -1;
                        } else if(strstr(rewrite_target(request.target), "../") != NULL) {
                            stats -> served_requests++;
                            stats -> ko_403++;
				        	char *message = "Forbidden\r\n";
				        	send_response(client, 403, "Forbidden", strlen(message), message);
				        	fclose(client);
				        	return -1;
			        	} else if(strcmp(rewrite_target(request.target), "/stats") == 0) {
                            stats -> served_requests++;
					        stats -> ok_200++;
					        skip_headers(client);
					        send_stats(client);
				        } else if(request.method == HTTP_UNSUPPORTED) {
                            stats -> served_requests++;
					        char *message = "Method Not Allowed\r\n";
			 		        send_response(client, 405, "Method Not Allowed", strlen(message), message);
					        fclose(client);
					        return -1;
                        }
			        	char *target;
				        target = rewrite_target(request.target);
				        FILE *f;
				        if((f = check_and_open(target, argv[1])) == NULL) {
                            stats -> served_requests++;
                            stats -> ko_404++;
				         	char *message = "Not Found\r\n";
					        send_response(client, 404, "Not Found", strlen(message), message);
					        fclose(client);
					        return -1;
				        } else {
                            stats -> served_requests++;
                            stats -> ok_200++;
					        skip_headers(client);
					        printf("Succès !\n");
					        int description_f = fileno(f);
					        send_response(client, 200, "OK", get_file_size(description_f), "");
					        copy(f, client);
				        }
				        fclose(f);
                        exit(0);
			        }
		        }
	        }
        }
        fclose(client);
    }
    return 0;
}

