#ifdef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <sys/select.h>
#include <signal.h>
#include <sys/ioctl.h>
//#pragma pack(pop)

int running = 1;


#define PORT 8080
#define BUFFER_SIZE 100
#define MULTICAST_PORT 5007

typedef struct {
    const char *name;
    int *language;
} getinfo;

struct thread_args {
    int sock;
    struct sockaddr_in addr;
    int alive;
    int language;
    int id;
    char name[50];
};

typedef struct{
    int type;
    int id;
    int laguage;
    size_t size_payload;
    char payload[BUFFER_SIZE*10];
}struct_message;

typedef struct{  //TODO: cheack the sizes
    char payload_bin[BUFFER_SIZE*10];
    char payload_hex[BUFFER_SIZE*4];
    char payload_ascii[BUFFER_SIZE + 10];
    char payload_oct[BUFFER_SIZE*8];
}struct_multicast_message;

struct_message new_user;

typedef struct{
    int type;
    char* name[50];
    int language;
}login;

enum State {
    STATE_STATEMENT = 1,
    STATE_LOGIN = 2,
    STATE_LOGOUT = 3,
    STATE_CONNECTED = 4,
    STATE_INNITIAL = 5,
};

enum command {
    LOGIN = 1,
    Get_Multicast = 2,
    JOIN_MASSAGE = 3,
    SEND_CHAT_MASSAGE = 4,
    GET_LIST = 5,
    ACK_EXIT = 8,
    autentication = 9,
    multicast_indicator = 10,
    ack = 11,
    
    NONE = 0
};

enum State currentState = STATE_INNITIAL;
enum State command = NONE;
struct thread_args multicast_struct;


struct thread_args client_struct;
int client_socket;
pid_t main_t;
pthread_t chat_massage;
pthread_t keep_alive;
struct_message alive;
int counter_not_reciev = 0;


int pipefd[2];


void handle_sigtstp(int signo) {
    int enable = 0;
    // if(setsockopt(multicast_struct.sock, IPPROTO_IP, IP_DROP_MEMBERSHIP,&enable, sizeof(enable))<0){
    //     perror("Error leaving multicast group");
    // }
    client_struct.alive = 0;
    multicast_struct.alive = 0;
    sleep(2);
    if (multicast_struct.sock != -1){
        close(multicast_struct.sock);
    }
    close(client_struct.sock);
    exit(0);
}

void getUserInfo(char *name, int* language);
void *receive_messages(void *socket);
void send_message(const char *message, int socket);
void setup_multicast_receiver(char *ip);

void send_message(const char *message, int socket) {//unicast
    write(socket, message, sizeof(message));
}

void clear_socket_buffer(int sockfd) {
    int bytes_available;
    ioctl(sockfd, FIONREAD, &bytes_available);

    char buffer[bytes_available];
    recv(sockfd, buffer, bytes_available, MSG_DONTWAIT);
}





//---------------------------encode----------------------

void encode_to_binary(const char *input, char *binaryBuffer) {
    int j = 0;
    int i;
    for (i = 0; input[i] != '\0'; i++) {
        int b;
        for (b = 7; b >= 0; b--) {
            binaryBuffer[j++] = ((input[i] >> b) & 1) ? '1' : '0';
        }
        binaryBuffer[j++] = ' ';
    }
    binaryBuffer[j] = '\0';
}


void encode_to_hexadecimal(const char *input, char *decodedBuffer) {
    // Initialize the buffer
    decodedBuffer[0] = '\0';

    // Variable to track the position in the output buffer
    int pos = 0;

    // Loop through each character in the input string
    int i;
    for (i = 0; input[i] != '\0'; i++) {
        // Convert the character to hex and store it in the output buffer
        pos += sprintf(decodedBuffer + pos, "%02X", (unsigned char)input[i]);

        // Add a space if there's room for it
        if (input[i + 1] != '\0') {
            decodedBuffer[pos] = ' ';
            pos++;
        }
    }

    // Null-terminate the string
    decodedBuffer[pos] = '\0';
}

void encode_to_ascii(const char *input, char *asciiBuffer) {
    strcpy(asciiBuffer, input); // Direct copy
 
}

void encode_to_octal(const char *input, char *decodedBuffer) {
    // Initialize the buffer
    decodedBuffer[0] = '\0';

    // Variable to track the position in the output buffer
    int pos = 0;

    // Loop through each character in the input string
    int i;
    for (i = 0; input[i] != '\0'; i++) {
        // Convert the character to octal and store it in the output buffer
        pos += sprintf(decodedBuffer + pos, "%03o", (unsigned char)input[i]);

        // Add a space if there's room for it
        if (input[i + 1] != '\0') {
            decodedBuffer[pos] = ' ';
            pos++;
        }
    }

    // Null-terminate the string
    decodedBuffer[pos] = '\0';
}

//void encode_to_octal(const char *input, char *octalBuffer) {
    //int j = 0;
    //for (int i = 0; input[i] != '\0'; i++) {
        //j += snprintf(octalBuffer + j, sizeof(octalBuffer) - j, "%03o ", (unsigned char)input[i]);
    //}
//}



//----------------------------------decode----------------------
void decode_from_binary(const char* encoded, char* decoded) {
    size_t length = strlen(encoded);
    size_t byte_count = length / 8;
    size_t i;
    for (i = 0; i < byte_count; ++i) {
        decoded[i] = 0;
        int j;
        for (j = 0; j < 8; ++j) {
            decoded[i] <<= 1;
            if (encoded[i * 8 + j] == '1') {
                decoded[i] |= 1;
            }
        }
    }
    decoded[byte_count] = '\0'; // Null-terminate the decoded string
}

void decode_from_hexadecimal(const char* encoded, char* decoded) {
    size_t i;
    for (i = 0; i < strlen(encoded) / 2; ++i) {
        sscanf(&encoded[i * 2], "%2hhX", (unsigned char*)&decoded[i]);
    }
    decoded[strlen(encoded) / 2] = '\0';
}

void decode_from_ascii(const char* encoded, char* decoded) {
    strcpy(decoded, encoded);
}

void decode_from_octal(const char* encoded, char* decoded) {
    size_t i;
    for (i = 0; i < strlen(encoded) / 3; ++i) {
        char temp[4] = {0};
        strncpy(temp, &encoded[i * 3], 3);
        decoded[i] = (char)strtol(temp, NULL, 8);
    }
    decoded[strlen(encoded) / 3] = '\0';
}
//--------------------------------------------------------------





void *periodic_keep_alive(void *arg){
    //int running = 1;
    int sockfd = *(int *)arg;
    alive.id = client_struct.id;
    alive.laguage = client_struct.language;
    strcpy(alive.payload, "");
    alive.size_payload = 0;
    alive.type = 7;
     while (client_struct.alive) {

        write(sockfd, &alive, sizeof(alive));
        // Send the multicast message
        //char msg[]="7";
        //send_message(msg,sockfd);

        // Wait for 5 seconds using select before sending the next message
        struct timeval tv;
        tv.tv_sec = 30;
        tv.tv_usec = 0;
        select(0, NULL, NULL, NULL, &tv);
    }
    printf("keep alive thread close");
    pthread_exit(NULL);
}

void print_menu() {
    printf("Welcome to Chat!\n");
    printf("We invite you to send messages.\n");
    printf("If you want to see who is logged in, please enter LIST.\n");
    printf("If you want to exit, please enter EXIT.\n");
    printf("Otherwise, send a message.\n");
}

// void clculate_size(char* msg, int* size) {
//     int language = client_struct.language;

//     if (language == 0) {
//         *size = strlen(msg) * 8;  // Size based on 8 times the length of msg
//     } else if (language == 1) {
//         *size = strlen(msg);      // Size based on the length of msg
//     } else if (language == 2) {
//         *size = strlen(msg) * 2;  // Size based on 2 times the length of msg
//     } else {
//         *size = strlen(msg) * 4;  // Size based on 4 times the length of msg
//     }
// }


void translate(char* msg, char* encoded){
    size_t size = sizeof(msg);
    if (client_struct.language == 0){//binary
        encode_to_binary(msg, encoded);
        return;
    }
    if (client_struct.language == 1){//ascii
        encode_to_ascii(msg, encoded);
        
        return;
    }
    if (client_struct.language == 2){//hexa
        encode_to_hexadecimal(msg, encoded);
        //printf(encoded);
        return;
    }
    encode_to_octal(msg, encoded); //octali
    return;
}

void *send_chat_message(void *arg) {
    
    int sockfd = *(int *)arg;

    while (running) {
        // Allocate memory for the message
        char *message = malloc(BUFFER_SIZE);
        if (message == NULL) {
            perror("malloc");
            return NULL; // Handle memory allocation error
        }
        memset(message, 0, BUFFER_SIZE);
        // Get user input
        //printf("Enter a message: \n");

        if (fgets(message, BUFFER_SIZE, stdin) != NULL) {
            message[strcspn(message, "\n")] = 0; // Remove newline

            if (strlen(message) > 0) {

                    if (strcmp(message, "EXIT") == 0) {
                    struct_message exit;
                    exit.id = client_struct.id;
                    exit.laguage = client_struct.language;
                    strcpy(exit.payload, "");
                    exit.size_payload = 0;
                    exit.type = 6;
                    write(sockfd, &exit, sizeof(exit));
                    running = 0;
                    client_struct.alive = 0;
                    multicast_struct.alive=0;


                } else if (strcmp(message, "LIST") == 0) {
                    struct_message list;
                    list.id = client_struct.id;
                    list.laguage = client_struct.language;
                    strcpy(list.payload, "");
                    list.size_payload = 0;
                    list.type = 5;
                    write(sockfd, &list, sizeof(list));
                    //sprintf(message, "5"); // Sending a command to list users
                    //send_message(message, sockfd);
                    memset(message, 0, BUFFER_SIZE);


                } else {
                    // Allocate memory for the buffer
                    int buffer_size = strlen(message) + 2; // +2 for command and null terminator
                    char *buffer = malloc(buffer_size);
                    if (buffer == NULL) {
                        perror("malloc");
                        free(message);
                        continue; // Handle memory allocation error
                    }

                    //sprintf(buffer, "4%s", message); // Prefix message type
                    //int size;
                    //clculate_size(message,&size);
                    char encoded_msg[BUFFER_SIZE*8];
                    int opt = 1;
                    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
                    translate(message, encoded_msg);
                    struct_message msg;
                    msg.type = 4;
                    msg.id = client_struct.id;
                    msg.laguage = client_struct.language;
                    strcpy(msg.payload, encoded_msg);
                    msg.size_payload = strlen(encoded_msg);
                    printf("you send: %s \n",msg.payload);
                    write(sockfd, &msg, sizeof(msg));
                    clear_socket_buffer(sockfd);
                    //send_message(buffer, sockfd);
                    
                    // Free the allocated buffer
                    free(buffer);
                }
            }
        }

        // Free the message after use
        if (strcmp(message, "EXIT") != 0){printf("Enter a message: \n");}
        free(message);
        
    }
    printf("finish send chat message, thread close\n");
    return NULL;
}

void get_write_trans(char* encoded, struct_multicast_message recv){
    if (client_struct.language == 0){//binary
        strcpy(encoded, recv.payload_bin);
        return;
    }
    if (client_struct.language == 1){//ascii
        strcpy(encoded, recv.payload_ascii);
        return;
    }
    if (client_struct.language == 2){//hexa
        strcpy(encoded, recv.payload_hex);
        return;
    }
    strcpy(encoded, recv.payload_oct); //octali
    return;
}



void *receive_multicast_thread(void *arg) {
    struct thread_args *args = (struct thread_args *)arg;
    int sock = args->sock;
    struct sockaddr_in addr = args->addr;
    socklen_t addrlen = sizeof(addr);
    struct_multicast_message recv;
    fd_set readfds;
    struct timeval timeout;

    while (multicast_struct.alive) {
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);

        // Set timeout
        timeout.tv_sec = 600;  // Timeout in seconds
        timeout.tv_usec = 0;

        // Wait for activity on socket
        int activity = select(sock + 1, &readfds, NULL, NULL, NULL);

        if (activity < 0) {
            perror("Select error");
            break;
        } else if (activity == 0) {
            printf("Timeout occurred.\n");
            continue;
        } else {
            // Check if socket has data to read
            if (FD_ISSET(sock, &readfds)) {
                // Allocate memory for the message
                char *message = malloc(1024); // Initial allocation, adjust as needed
                if (message == NULL) {
                    perror("malloc error");
                    break;
                }
                int bytes_received = recvfrom(sock, &recv, sizeof(recv), 0, (struct sockaddr *) &addr, &addrlen);
                counter_not_reciev = 0;
                //int bytes_received = recvfrom(sock, message, 1024, 0, (struct sockaddr *) &addr, &addrlen);
                if (bytes_received < 0) {
                    perror("Recvfrom error");
                    free(message);
                    break;
                } else {
                    // Handle received message
                    char trans_msg[8*BUFFER_SIZE];
                    get_write_trans(trans_msg, recv);
                    trans_msg[strlen(trans_msg)] = '\0'; //TODO: maybe without
                    //message[bytes_received] = '\0'; // Null-terminate the message
                    printf("Received: %s\n", trans_msg);

                    // Process the message here
                    // ...

                    // Free the allocated memory
                    free(message);
                }
            }
        }
    }
    printf("leav multicast\n");
    close(multicast_struct.sock);

    return NULL;
}


void parse_multicast_ip_and_join(const char *buffer) {
    char ip_address[INET_ADDRSTRLEN];
    int num_chars_read;

    // Skip the first character in the buffer
     // Move pointer to skip the first character

    // Scan the buffer for the multicast IP address
    num_chars_read = sscanf(buffer, "%s", ip_address);

    if (num_chars_read == 1) {
        // Print the extracted multicast IP address
        printf("Multicast IP Address: %s\n", ip_address);
        setup_multicast_receiver(ip_address);

        // Convert the IP address from string to network address structure
        struct in_addr addr;
        if (inet_pton(AF_INET, ip_address, &addr) <= 0) {
            perror("inet_pton");
            return;
        }

        // Example usage: print the IP address in numeric format
        printf("Numeric IP Address: %s\n", inet_ntoa(addr));
    } else {
        printf("Failed to extract multicast IP address from buffer.\n");
    }
}


void setup_multicast_receiver(char* ip) {
    struct ip_mreq mreq;
    int sock;
    struct sockaddr_in addr;
    pthread_t thread;
    

    // Create socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }



    int enable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(enable)) < 0) {
        perror("setsockopt(SO_REUSEPORT) failed");
        exit(EXIT_FAILURE);
    }
        // Set up socket address
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(MULTICAST_PORT);

    // Bind socket
    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        perror("Bind failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Set up multicast group
    mreq.imr_multiaddr.s_addr = inet_addr(ip);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        perror("Setsockopt IP_ADD_MEMBERSHIP failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Set up thread arguments
    multicast_struct.sock = sock;
    multicast_struct.addr = addr;

    // Create receive thread
    multicast_struct.alive=1;
    if (pthread_create(&thread, NULL, receive_multicast_thread, (void *)&multicast_struct) != 0) {
        perror("Thread creation failed");
        close(sock);
        exit(EXIT_FAILURE);
    }
    

    // Main thread can continue or do other tasks if needed
    printf("youre in the chat.\n");
    char buffer[BUFFER_SIZE];
    //establish

    //TODO need to remember close socket
}

void getUserInfo(char* name, int* language) {
    
    printf("Enter your name: \n");
    fflush(stdout); // Ensure prompt is printed immediately

    // Set up select for input
    fd_set read_fds;
    struct timeval tv;
    tv.tv_sec = 30;  // 10 seconds timeout
    tv.tv_usec = 0;

    FD_ZERO(&read_fds);
    FD_SET(STDIN_FILENO, &read_fds);
    int retval;

    retval = select(STDIN_FILENO + 1, &read_fds, NULL, NULL, &tv);

    if (retval == -1) {
        perror("select()");
        exit(EXIT_FAILURE);
    } else if (retval == 0) {
        printf("\nInput timed out!\n");
         struct_message no_log;
        no_log.id = client_struct.id;
        no_log.laguage = 0;
        strcpy(no_log.payload, "");
        no_log.size_payload = 0;
        no_log.type = 9;
        client_struct.alive = 0;
        multicast_struct.alive=0;
        write(client_socket, &no_log, sizeof(no_log));
        running =0;
        close(client_socket);
        return ;
    }
    // Prompt user to enter name
    
    scanf("%s", name); // Read name from user input
    
    // Prompt user to select a language
    printf("Enter a number for your language from the list:\n");
    printf("0 : binary, 1 : english, 2 : exadecimal , 3 : octal\n");

    FD_ZERO(&read_fds);
    FD_SET(STDIN_FILENO, &read_fds);
    fflush(stdout);

    retval = select(STDIN_FILENO + 1, &read_fds, NULL, NULL, &tv);

    if (retval == -1) {
        perror("select()");
        exit(EXIT_FAILURE);
    } else if (retval == 0) {
        printf("\nInput timed out!\n");
          struct_message no_log;
        no_log.id = client_struct.id;
        no_log.laguage = 0;
        strcpy(no_log.payload, "");
        no_log.size_payload = 0;
        no_log.type = 9;
        write(client_socket, &no_log, sizeof(no_log));
        running =0;
        client_struct.alive = 0;
        multicast_struct.alive=0;
        close(client_socket);
        return ;
    }
    // Prompt user to enter name
    scanf("%d", language); // Read language choice from user input
    client_struct.language = *language;
    strcpy(client_struct.name, name);
   
    
}

void *receive_messages(void *socket) {//unicast
    int client_socket = *(int *)socket;
    char buffer[BUFFER_SIZE];
    int bytes_read;


    fd_set read_fds;
    FD_SET(client_socket, &read_fds);
    client_struct.alive = 1;
    

    //(bytes_read = read(client_socket, buffer, sizeof(buffer) - 1)) > 0
    struct_message recv;
    while (running) {
        struct timeval timeout;
        timeout.tv_sec = 60;  // Example: 10 minutes timeout
        timeout.tv_usec = 0;
        memset(buffer, 0, BUFFER_SIZE);
        int select_result = select(client_socket +1  , &read_fds, NULL, NULL, &timeout);

        if (select_result == -1) {
            perror("Select error");
            close(client_socket);
            pthread_exit(NULL);
        } else if (select_result == 0) {
            // Timeout occurred
            char timeout_message[] = "Login timeout no ack recievd. Please try again.\n";
            //send_message(timeout_message, client_socket);
            running = 0;
            client_struct.alive = 0;
            multicast_struct.alive=0;
            sleep(0.5);
            close(client_socket);
            close(multicast_struct.sock);
            pthread_exit(NULL);
        }
        

        // Now check if there is data to read
        if (FD_ISSET(client_socket, &read_fds)) {
            // Handle login process
            int bytes_read;
            bytes_read = read(client_socket, &recv, sizeof(recv));

        





        //buffer[bytes_read] = '\0'; // Ensure null-termination
        //char c = buffer[0];
        //char num_str[2];  // Buffer to hold the character and null terminator

        //num_str[0] = c;   // Copy the character into the buffer
        //num_str[1] = '\0'; // Null-terminate the string

        //int cmd = atoi(num_str); 
        int command = recv.type;
        //printf("command: %d \n", command);
        //printf("debugger: Received message: %s\n", buffer);
        if (command == 0){break;}
        char name[50]; // Assuming name won't exceed 50 characters
        int language;
        fd_set readfds;
        struct timeval timeout;
        if (bytes_read == 0){
            
           exit(0);
        }
        switch (command) { // Cast cmd to enum State
            case LOGIN:
                

                printf("send LOGIN \n");
                client_struct.id = recv.id;
                getUserInfo(name, &language);
                sprintf(buffer, "1LOGIN: %s %d", name, language);
                client_struct.language = language;
                strcpy(client_struct.name, name);
                
                struct_message log;
                log.id = client_struct.id;
                log.laguage = language;
                strcpy(log.payload, name);
                log.size_payload = sizeof(log.payload);
                log.type = 1;
                write(client_socket, &log, sizeof(log));
                break;

            case ACK_EXIT:
                 kill(main_t,SIGTSTP); 



            case Get_Multicast:
                printf("Get multicast\n");

                parse_multicast_ip_and_join(recv.payload);
                struct_message join;
                join.id = client_struct.id;
                join.laguage = language;
                strcpy(join.payload, "");
                join.size_payload = 0;
                join.type = 3;
                write(client_socket, &join, sizeof(join));

                //char command_num[] = "3";
                printf("send join massage\n");
                //write(client_socket, command_num, strlen(buffer));
                fflush(stdout);
                print_menu();

                
                if (pthread_create(&chat_massage, NULL, send_chat_message, &client_socket) != 0) {
                    perror("Thread creation failed");
                    exit(EXIT_FAILURE);}

                
                if (pthread_create(&keep_alive, NULL, periodic_keep_alive, &client_socket) != 0) {
                    perror("Thread creation failed");
                    exit(EXIT_FAILURE);}
                

                

                break;


            case GET_LIST:

                printf("GET LIST\n");
                //char* ptr = buffer + 1;
                //strcpy(r_list, recv.payload);
                //printf("%s",ptr);
                printf("%s",recv.payload);

                break;

            case autentication:
                kill(main_t,SIGTSTP); 
                break;    
           
            case multicast_indicator:
            
            FD_ZERO(&readfds);
            FD_SET(multicast_struct.sock, &readfds);

                // Set timeout
                timeout.tv_sec = 5;  // Timeout in seconds
                timeout.tv_usec = 0;

                // Wait for activity on socket
                int activity = select(multicast_struct.sock + 1, &readfds, NULL, NULL, &timeout);

                if (activity < 0) {
                    perror("Select error");
                    break;
                 } else if (activity == 0) {

                    printf("message didnt arrive.\n");
                    counter_not_reciev++;
                    if (counter_not_reciev > 5){
                        printf("more than 5 message didnt arrive, close socket.\n");
                        struct_message exit;
                        exit.id = client_struct.id;
                        exit.laguage = client_struct.language;
                        strcpy(exit.payload, "");
                        exit.size_payload = 0;
                        exit.type = 6;
                        write(client_socket, &exit, sizeof(exit));
                        running = 0;
                        client_struct.alive = 0;
                        multicast_struct.alive=0;
                        
                    }
                    
                } else {
                        // Check if socket has data to read
                        if (FD_ISSET(multicast_struct.sock, &readfds)) {
                            printf("multicast arriving\n");
                
                        }
                    }
                break;
            case ack:
            printf("ack\n");
            break;
            default:
                printf("Unknown state\n");
                break;
        }
    }

    
}

return NULL;
}




int main() {
    main_t = getpid();
    signal(SIGTSTP, handle_sigtstp);
    multicast_struct.sock = -1;

    
    struct sockaddr_in server_addr;
    pthread_t tid;
    char buffer[BUFFER_SIZE];
    
    

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    int enable;
    if (setsockopt(client_socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &enable, sizeof(enable))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    client_struct.sock = client_socket;
    //client_struct.addr = server_addr;

    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {//make tcp connection
        perror("Connect failed");
        close(client_socket);
        exit(EXIT_FAILURE);
    }
    

    
    
    if (pthread_create(&tid, NULL, receive_messages, (void *)&client_socket) != 0) {
        perror("Thread creation failed");
        close(client_socket);
        exit(EXIT_FAILURE);
    }
    else{
    currentState = STATE_LOGIN;
    }    

    while (running) {
        switch (currentState) {
        case STATE_LOGIN:
            //printf("Current state: login\n");
            break;
        case STATE_LOGOUT:
            printf("Current state: Logout\n");
            break;
        case STATE_CONNECTED:
            printf("Current state: Connected\n");
            break;
        default:
            printf("Unknown state\n");
            break;
    }

        //fgets(buffer, sizeof(buffer), stdin);
        //write(client_socket, buffer, strlen(buffer));
    }
    //printf("main\n");
    close(client_socket);
    return 0;
}
