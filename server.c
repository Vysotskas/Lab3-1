#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h> 

#define MAX_FILEPATH_SIZE 256
#define MAX_DATA_BLOCK_SIZE 256
#define PORT 3456
#define MAX_THREAD_COUNT 1
#define MAX_QUEUE_COUNT 5

void* thread_function(void *param);

int main(int argc, char **argv)
{
	int server_socket_id = 0;
	int client_id = 0;
	struct sockaddr_in server_socket;
	printf("Server started...\n");

#ifdef THREAD	
	pthread_t threads[MAX_THREAD_COUNT] = {0};
	int i = 0;
#elif defined (PROCESS)
	pid_t process_id = 0;
#endif
	if ((server_socket_id = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		printf("Error occured while openning socket.\n");
		return -1;
	}

	server_socket.sin_family = AF_INET;
	server_socket.sin_addr.s_addr = INADDR_ANY;
	server_socket.sin_port = htons(PORT);

	if (bind(server_socket_id, (struct sockaddr *)&server_socket, sizeof(server_socket)) < 0){
		printf("Error occured while binding socket to address.\n");
		return -2;
	}
	
	if (listen(server_socket_id, MAX_QUEUE_COUNT) < 0){
		printf("Error occured while trying to start listening socket.\n");
		return -3;
	}

	while (1){
		client_id = accept(server_socket_id, NULL, (socklen_t *)SOCK_CLOEXEC);

		if (client_id < 0){
			printf("Error occured while accepting a client.\n");
			continue;
		}
#ifdef THREAD
		for (i = 0; i < MAX_THREAD_COUNT; i++)
			if ((threads[i] == 0) || (pthread_kill(threads[i], 0) == ESRCH))
				break;

		if (i >= MAX_THREAD_COUNT){
			printf("No free threads.\n");
			continue;
		}

		if (pthread_create(&threads[i], NULL, thread_function, (void*)client_id)){
			printf("Thread wasn't created.\n");
			continue;
		}

		printf("Thread was created.\n");
#elif defined(PROCESS)
		switch (process_id = fork()){
		case -1:
			printf("Process creation failed.\n");
			break;
		case 0:
			thread_function((void*)client_id);
			return 0;
		default:
		  	close(client_id);
			break;
		}
#else
		thread_function((void*)client_id);
#endif			
	}

	return 0;
}

void* thread_function(void *param)
{
	char file_path[MAX_FILEPATH_SIZE];
	unsigned char data_block[MAX_DATA_BLOCK_SIZE];
	int bytes_send = 0;
	int bytes_read = 0;
	int client_id = (int)param;
	FILE *file;
	memset(data_block, 0, sizeof(data_block));

	bytes_read = read(client_id, file_path, sizeof(file_path) - 1);

	if (bytes_read == -1){
		printf("Error occured while reading filepath.\n");
		close(client_id);
		return;
	}

	file_path[bytes_read] = 0;

	if (access(file_path, F_OK) == -1){
		printf("File not found.\n");
		close(client_id);
		return;
	}

	file = fopen(file_path, "rb");

	if (file == NULL){
		printf("Error occured while trying to open file.\n");
		close(client_id);
		return;
	}

	while (1){
		bytes_read = fread(data_block, 1, MAX_DATA_BLOCK_SIZE, file);
	
		if (bytes_read > 0){
			bytes_send = write(client_id, data_block, bytes_read);
			if (bytes_send < bytes_read)
				printf("Error occured while sending file.\n");
		}

		if (bytes_read < MAX_DATA_BLOCK_SIZE){
			if (feof(file))
				printf("File was sent.\n");
			if (ferror(file))
				printf("Error occured while reading from file.\n");
			break;
		}
	}
	fclose(file);
	close(client_id);
}