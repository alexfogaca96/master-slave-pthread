#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define BUFFER_SIZE 256
#define IPV4 AF_INET
#define TCP SOCK_STREAM
#define address_in struct sockaddr_in

/* Mensagens de término do programa */
const char exit_message[] = "exit\n";

/* Porta de conexão ao server */
int PORT;
char HOST[9] = "127.0.0.1";

/* Tira os '\n' do stdin, que sobraram do scanf */
void clear_stdin()
{
	while (1) {
		int ch = getchar();
		if(ch == '\n' || ch == EOF) {
			break;
		}
	}
}

/*
 * Troca mensagens com o servidor por meio do socket
 * configurado com IPV4 e TCP.
 */
void chat_with_server(int socket_file_descriptor)
{
	char buffer[BUFFER_SIZE];
	clear_stdin();
	printf("To terminate the application, type exit.\n");
	while(1) {
		// recebe texto do terminal e manda para o servidor pelo socket
		printf("Enter the message, please (limit %d characters): ", BUFFER_SIZE);
		fgets(buffer, BUFFER_SIZE, stdin);
		if(write(socket_file_descriptor, buffer, strlen(buffer) - 1) < 0) {
			fprintf(stderr, "Couldn't write to socket.\n");
			exit(0);
		}

		// se o texto digitado for 'exit', termina aplicação
		if(strcmp(exit_message, buffer) == 0) {
			printf("Bye!\n");
			exit(0);
		}

		// lê resposta do servidor
		memset(buffer, 0, BUFFER_SIZE - 1);
		if(read(socket_file_descriptor, buffer, BUFFER_SIZE - 1) < 0) {
			fprintf(stderr, "Couldn't read from socket.\n");
			exit(0);
		}
		printf("%s\n", buffer);
	}
	close(socket_file_descriptor);
}

/*
 * Configura socket para se conectar com o servidor, tenta a conexão
 * e retorna o número da conexão para poder ser acessada.
 */
int create_server_connection()
{
	// configura socket IPV4 e TCP
	int socket_file_descriptor = socket(IPV4, TCP, 0);
	if(socket_file_descriptor < 0) {
		fprintf(stderr, "Couldn't open socket.\n");
		exit(0);
	}

	// procura o host pelo nome (127.0.0.1 ou localhost)
	struct hostent* server = gethostbyname(HOST);
	if(server == NULL) {
		fprintf(stderr, "No such host is available.\n");
		exit(0);
	}

	address_in server_address;
	bzero((char*) &server_address, sizeof(server_address));
	server_address.sin_family = IPV4;
	bcopy((char*) server->h_addr,
		(char*) &server_address.sin_addr.s_addr,
		server->h_length);
	server_address.sin_port = htons(PORT);

	// tenta conectar ao servidor
	int connection = connect(
		socket_file_descriptor,
		(struct sockaddr*) &server_address,
		sizeof(server_address));
	if(connection < 0) {
		fprintf(stderr, "Couldn't connect to server.\n");
		exit(0);
	}

	return socket_file_descriptor;
}

/* Inicializa todos parâmetros pela leitura no terminal */
void set_parameters()
{
    printf("You need to set some parameters before the execution.\n");

    char raw_port[4];
    printf("> port number desired: ");
    scanf("%s", raw_port);
    PORT = atoi(raw_port);
}

/*
 * Ponto de execução do cliente:
 * - recebe parâmetros iniciais;
 * - cria conexão socket com servidor;
 * - troca mensagens com o servidor.
 */
int main()
{
	set_parameters();
	int socket_file_descriptor = create_server_connection();
	chat_with_server(socket_file_descriptor);
}
