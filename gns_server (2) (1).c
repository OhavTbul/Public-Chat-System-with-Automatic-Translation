
#ifdef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <netinet/in.h> // Include for struct ip_mreq
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/select.h> // Add this for select function
#include <signal.h>
#include <sys/ioctl.h>
//#pragma pack(pop)



#define PORT 8080
#define BUFFER_SIZE 100
#define MAX_CLIENTS 10
#define MULTICAST_GROUP "239.0.0.1"
#define MULTICAST_PORT 5007
//int running= 1;
int running_multicast = 1;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition = PTHREAD_COND_INITIALIZER;

char buffer[BUFFER_SIZE];
int first_connection =0;
int connected =0;
const char *languages[] = {"binary", "english", "hexdecimal", "octali"};
int server_socket;
pid_t main_t;

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

// struct ip_mreq {
//     struct in_addr imr_multiaddr;   // Multicast group IP address
//     struct in_addr imr_interface;    // Local interface address
// };

enum command {
    LOGIN = 1,
    Get_Multicast = 2,
    JOIN_MASSAGE = 3,
    SEND_MASSAGE = 4,
    LIST = 5,
    EXIT = 6,
    KEEP_ALIVE = 7,
    ACK_EXIT = 8,
    NO_LOG =9,
    NONE = 0

};

typedef struct {
    int socket;
    int id;
    char name[30];
    int language;
    char name_langugae[20];
} User;


struct multicast_struct {
    int sockfd; // Socket file descriptor
    struct sockaddr_in multicast_addr; // Multicast address structure
    char multicast_ip[16]; // Multicast IP address as string
};




User clients[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
struct multicast_struct mult;
int availible_language[4] = {0,0,0,0};

int login_user(User *user, struct_message);
void send_message(const char *message, int socket);
void broadcast_message(const char *message, int sender_socket);
void remove_client(int socket);




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
   // int j = 0;
      int i;
    //for (i = 0; input[i] != '\0'; i++) {
      //  j += snprintf(octalBuffer + j, sizeof(octalBuffer) - j, "%03o ", (unsigned char)input[i]);
    ////}
//}



//----------------------------------decode----------------------
void decode_from_binary(const char* input, char* decoded) {
    size_t index = 0;
    size_t bit_count = 0;

    // Process the input and decode
    size_t i;
    for (i = 0; input[i] != '\0'; ++i) {
        if (input[i] != ' ') {
            if (bit_count % 8 == 0) {
                decoded[index++] = 0; // Move to the next byte
            }
            decoded[index - 1] <<= 1;
            if (input[i] == '1') {
                decoded[index - 1] |= 1;
            }
            bit_count++;
        }
    }
    decoded[bit_count / 8] = '\0'; // Null-terminate the decoded string
}



void decode_from_hexadecimal(const char *hexInput, char *outputBuffer, size_t bufferSize) {
    // Initialize the output buffer
    outputBuffer[0] = '\0';
    
    // Temporary buffer for conversion
    char temp[3] = {0};
    size_t pos = 0;
    
    // Process the hex input
    size_t i;
    for (i = 0; i < strlen(hexInput); i += 3) {
        // Get two characters for hex value
        temp[0] = hexInput[i];
        temp[1] = hexInput[i + 1];

        // Convert hex to decimal and store in output buffer
        if (pos + 1 < bufferSize) {
            outputBuffer[pos++] = (char)strtol(temp, NULL, 16);
        }
    }
    outputBuffer[pos] = '\0'; // Null-terminate the string
}

void decode_from_ascii(const char* encoded, char* decoded) {
    strcpy(decoded, encoded);
}

// void decode_from_octal(const char* encoded, char* decoded) {
//     for (size_t i = 0; i < strlen(encoded) / 3; ++i) {
//         char temp[4] = {0};
//         strncpy(temp, &encoded[i * 3], 3);
//         decoded[i] = (char)strtol(temp, NULL, 8);
//     }
//     decoded[strlen(encoded) / 3] = '\0';
// }

void decode_from_octal(const char *input, char *decodedBuffer) {
    // Initialize the buffer
    decodedBuffer[0] = '\0';

    // Variable to track the position in the output buffer
    int pos = 0;

    // Temporary variable to hold the current octal number
    char octal[4];
    
    // Loop through the input string
    int i;
    for (i = 0; input[i] != '\0';) {
        // Read the next three characters (octal number)
        strncpy(octal, input + i, 3);
        octal[3] = '\0'; // Null-terminate the octal string

        // Convert the octal string to an integer and then to a character
        int octalValue = strtol(octal, NULL, 8);
        decodedBuffer[pos++] = (char)octalValue;

        // Move to the next part, skipping the space if it's there
        i += 4; // Move past the octal number and the space
    }

    // Null-terminate the decoded string
    decodedBuffer[pos] = '\0';
}
//--------------------------------------------------------------


char* create_user_list(int *total_size) {
    // Initialize the message with a starting phrase
    const char *header = "5---------------------------------------\nThe user online chat is:\n";
    size_t buffer_size = strlen(header) + 1; // +1 for null terminator
    char *buffer = malloc(buffer_size);
    if (!buffer) {
        perror("malloc");
        return NULL;
    }
    strcpy(buffer, header);

    // Iterate over the users and build the message
    int i;
    for (i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].id != 0) {
            char user_entry[100];
            snprintf(user_entry, sizeof(user_entry), "user %d, %s, %s\n", clients[i].id, clients[i].name, clients[i].name_langugae );
            printf("user %d, %s, %s\n", clients[i].id, clients[i].name, clients[i].name_langugae);

            size_t user_entry_size = strlen(user_entry);
            buffer_size += user_entry_size;

            char *new_buffer = realloc(buffer, buffer_size);
            if (!new_buffer) {
                perror("realloc");
                free(buffer);
                return NULL;
            }
            buffer = new_buffer;
            strcat(buffer, user_entry);
        }
    }

    const char *footer = "---------------------------------------\n";
    buffer_size += strlen(footer);
    buffer = realloc(buffer, buffer_size);
    if (!buffer) {
        perror("realloc");
        free(buffer);
        return NULL;
    }
    strcat(buffer, footer);

    *total_size = buffer_size;
    return buffer;
}


void handle_sigtstp(int signo) {
    running_multicast = 0;
    close(mult.sockfd);
    int i =0;
    for(i=0;i<client_count;i++)
    {
        close(clients[i].socket);
    }
    close(server_socket);
    exit(0);
}

void *periodic_keep_alive(void *arg){  //TODO: we need to delete
    struct multicast_struct *args = (struct multicast_struct *)arg;
     while (1) {
        // Send the multicast message
        char msg[]="keep alive";
        if (sendto(args->sockfd, msg, strlen(msg), 0, (struct sockaddr *)&args->multicast_addr, sizeof(args->multicast_addr)) < 0) {
            perror("sendto");
        }

        // Wait for 5 seconds using select before sending the next message
        struct timeval tv;
        tv.tv_sec = 25;
        tv.tv_usec = 0;
        select(0, NULL, NULL, NULL, &tv);
    }
    printf("multicast socket closed");
    close(args->sockfd);
    pthread_exit(NULL);
}





void *multicast_server(void *arg) {
    struct sockaddr_in addr;
    int sockfd;
    ssize_t nbytes;
    char msg[BUFFER_SIZE];
    // Create a UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }

    // Set up multicast address
    strcpy(mult.multicast_ip, MULTICAST_GROUP); 
    memset(&addr, 0, sizeof(addr));
    mult.multicast_addr.sin_family = AF_INET;
    mult.multicast_addr.sin_addr.s_addr = inet_addr(mult.multicast_ip);
    mult.multicast_addr.sin_port = htons(MULTICAST_PORT);

    printf("Multicast server listening on %s:%d\n", MULTICAST_GROUP, MULTICAST_PORT);
    mult.sockfd = sockfd;


    int enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        exit(EXIT_FAILURE);
    }
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(enable)) < 0) {
        perror("setsockopt(SO_REUSEPORT) failed");
        exit(EXIT_FAILURE);
    }

    int ttl = 60;
    if (setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0) {
        perror("setsockopt(IP_MULTICAST_TTL) failed");
        exit(EXIT_FAILURE);
    }
    // Receive and process data
    while (running_multicast) {}

    close(sockfd);
}

void trans_message(struct_multicast_message* msg, char *payload)
{
    strcpy(msg->payload_ascii,"");
    strcpy(msg->payload_bin,"");
    strcpy(msg->payload_hex,"");
    strcpy(msg->payload_oct,"")
;
    if (availible_language[0] >0)
    {encode_to_binary(payload, msg->payload_bin );}

    if (availible_language[2] >0)
    {encode_to_hexadecimal(payload, msg->payload_hex);}

    if (availible_language[3] >0)
    {encode_to_octal(payload, msg->payload_oct );}

    if (availible_language[1] >0)
    {encode_to_ascii(payload, msg->payload_ascii ); }
                     
}

void send_message(const char *message, int socket) {//unicast
    write(socket, message, strlen(message));
}

void broadcast_message(const char *message, int sender_socket) {//TODO : need to change it to multicast
    pthread_mutex_lock(&clients_mutex);
    int i;
    for (i = 0; i < client_count; i++) {
        if (clients[i].socket != sender_socket) {
            send_message(message, clients[i].socket);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void remove_client(int socket) {
    pthread_mutex_lock(&clients_mutex);
    int i;
    for (i = 0; i < client_count; i++) {
        if (clients[i].socket == socket) {
            clients[i].id = 0;
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void decode_message(int format, const char* encoded, char* decoded) {
    switch (format) {
        case 0:
            decode_from_binary(encoded, decoded);
            printf("decode: %s",decoded);
            break;
        case 1:
            decode_from_ascii(encoded, decoded);
            break;
        case 2:
            decode_from_hexadecimal(encoded, decoded, sizeof(decoded));
            break;
        case 3:
            decode_from_octal(encoded, decoded);
            break;
        default:
            strcpy(decoded, "Invalid format");
            break;
    }
}

int login_user(User *user, struct_message rec_msg) {
    int bytes_read;
    //need to fix it
    // Handle client commands
    
        //buffer[bytes_read] = '\0';
        printf("User %d: %s\n", user->id, rec_msg.payload);

        // Check for login command
        if (rec_msg.type == 1) {
            // Parse login information
            char name[30]; // Adjust size as needed
            int language; // Adjust size as needed
            user->language = rec_msg.laguage;
            strcpy(user->name, rec_msg.payload);
            clients[user->id - 1].id = user->id;
            strcpy(clients[user->id - 1].name_langugae, languages[user->language]);
            strcpy(clients[user->id - 1].name, rec_msg.payload);
            clients[user->id - 1].language = user->language;
            availible_language[user->language] ++;
                

            
            strcpy(user->name_langugae, languages[user->language]);
                

            printf("User %d logged in with name '%s' and language '%s'\n", user->id, user->name, user->name_langugae);

                // Broadcast login message to other clients
                //char login_message[BUFFER_SIZE];
                //sprintf(login_message, "User %d has joined the chat with name '%s'", user->id, user->name);
                //broadcast_message(login_message, user->socket);

                return 1; // Successful login
            } else {
                // Invalid login format, notify client and continue loop
                char invalid_login_message[] = "Invalid login format. Please try again.\n";
                send_message(invalid_login_message, user->socket);
            }
        
    



    return 0; // Login failed or client disconnected
}

void clear_socket_buffer(int sockfd) {
    int bytes_available;
    ioctl(sockfd, FIONREAD, &bytes_available);

    char buffer[bytes_available];
    recv(sockfd, buffer, bytes_available, MSG_DONTWAIT);
}

void clean_input(const char* input, char* cleaned_input) {
    size_t index = 0;
    size_t i;
    for (i = 0; i < strlen(input); ++i) {
        if (input[i] != ' ') {
            cleaned_input[index++] = input[i];
        }
    }
    cleaned_input[index] = '\0'; // Null-terminate cleaned input
}



void *handle_client(void *arg) {
    User *user = (User *)arg;

    // Send initial login prompt
    struct_message login;
    login.id = user->id;
    login.type = 1;
    strcpy(login.payload,"");
    login.laguage = -1;
    login.size_payload = 0;


    char initial_prompt[] = "1";
    printf("send GET LOGIN command\n");
    //send_message(initial_prompt, user->socket);
    write(user->socket, &login, sizeof(login));// send login

    // Set timeout duration
    

    // Use select to wait for input from the client
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(user->socket, &read_fds);
    
    while (1) {
        //struct_message* rec_msg = malloc(sizeof(struct_message));
        // if (rec_msg == NULL) {
        //     perror("Failed to allocate memory");
        //     return -1;
        // }
        struct timeval timeout;
        timeout.tv_sec = 60;  // Example: 30 seconds timeout
        timeout.tv_usec = 0;
        struct_message rec_msg;
        fflush(stdout);
        int opt = 1;
        setsockopt(user->socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        clear_socket_buffer(user->socket);
        int select_result = select(user->socket + 1, &read_fds, NULL, NULL, &timeout);
        if (select_result == -1) {
            perror("Select error");
            close(user->socket);
            remove_client(user->socket);
            free(user);
            pthread_exit(NULL);
        } else if (select_result == 0) {
            // Timeout occurred
            char timeout_message[] = "Login timeout. \n";
            printf("keep alive timeout\n");
            struct_message auteticate;
            auteticate.id = user->id;
            auteticate.laguage = user->language;
            strcpy(auteticate.payload, "");
            auteticate.type = 9;
            auteticate.size_payload = 0;
            write(user->socket,&auteticate,sizeof(auteticate));
            sleep(1);
            close(user->socket);
            //remove_client(user->socket);
            free(user);
            pthread_exit(NULL);
        }

        // Now check if there is data to read
        if (FD_ISSET(user->socket, &read_fds)) {
            // Handle login process
            size_t buffer_size = BUFFER_SIZE;
            char *buffer = malloc(buffer_size);
            if (buffer == NULL) {
                perror("malloc");
                close(user->socket);
                remove_client(user->socket);
                free(user);
                pthread_exit(NULL);
            }
            
            int bytes_read;
            int size = 1024;
            setsockopt(user->socket, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
            int opt = 1;
            setsockopt(user->socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
            memset(buffer, 0, buffer_size);

            bytes_read = recv(user->socket, &rec_msg, sizeof(rec_msg),0);
 
            printf("%s\n", rec_msg.payload);
   
            int command = rec_msg.type;
            if (command == 0)
            {printf("break happend\n");
                break;}
            struct_message exit_ack;
            struct_message ack;
            switch (command) {
                case LOGIN:
                    login_user(user, rec_msg);
                    if (client_count == 1 && !first_connection) {
                        pthread_t multicast_tid;
                        int connect = 0;
                        if (pthread_create(&multicast_tid, NULL, multicast_server, &connect) != 0) {
                            perror("Thread creation failed");
                        }
                        sleep(2);   
                        first_connection = 1;
                    }

                    // Send the multicast IP in unicast to client
                    memset(buffer, 0, buffer_size); 
                    sprintf(buffer, "2%s", MULTICAST_GROUP);
                    
                    struct_message send_multicast_ip; 
                    send_multicast_ip.id = user->id;
                    send_multicast_ip.laguage = user->language;
                    send_multicast_ip.type = 2;
                    strcpy(send_multicast_ip.payload,MULTICAST_GROUP);
                    send_multicast_ip.size_payload = sizeof(MULTICAST_GROUP);
                     
                    write(user->socket, &send_multicast_ip, sizeof(send_multicast_ip));
                    fprintf(stdout, "sending multicast ip\n");
                    break;

                case JOIN_MASSAGE:
                    if (rec_msg.id == user->id){
                        printf("user autentication passed\n");

                    
                    struct_multicast_message join;
                    sprintf(buffer, "User %d logged in with name '%s' and language '%s'\n", user->id, user->name, user->name_langugae);
                    //printf(buffer);
                    trans_message(&join, buffer);
                    // encode_to_binary(buffer, join.payload_bin );
                    // encode_to_hexadecimal(buffer, join.payload_hex);
                    // encode_to_octal(buffer, join.payload_oct );
                    // encode_to_ascii(buffer, join.payload_ascii );
                    memset(buffer, 0, buffer_size); 
                    printf("SERVER GET JOIN MESSAGE\n");

                    //sprintf(buffer, "User %d logged in with name '%s' and language '%s'\n", user->id, user->name, user->name_langugae);
                    if (sendto(mult.sockfd, &join, sizeof(join), 0, (struct sockaddr *)&mult.multicast_addr, sizeof(mult.multicast_addr)) < 0) {
                        perror("sendto fail");
                    }
                    break;
                    }
                    else{
                        printf("user autentication failed\n");
                        struct_message auteticate;
                        auteticate.id = user->id;
                        auteticate.laguage = user->language;
                        strcpy(auteticate.payload, "");
                        auteticate.type = 9;
                        auteticate.size_payload = 0;
                        write(user->socket,&auteticate,sizeof(auteticate));
                        sleep(2);

                        if (client_count != 1){
                            pthread_mutex_lock(&clients_mutex);
                                client_count--;
                                availible_language[user->language]--;
                            pthread_mutex_unlock(&clients_mutex);
                            clients[user->id - 1].id = 0;
                            close(user->socket);

                        }
                        else{
                       // running = 0;
                        close(user->socket);
                       // close(mult.sockfd);
                        //exit(0);
                        }
                        


                    }

                case SEND_MASSAGE:
                    //char *ptr = buffer + 1;
                    if (rec_msg.id == user->id){
                    size_t message_size = snprintf(NULL, 0, "User %d,%s: %s\n", user->id, user->name, rec_msg.payload);
                    char *message = malloc(message_size);
                    if (message == NULL) {
                        perror("malloc");
                        break; // Handle error
                    }
                    char decoded[BUFFER_SIZE+10];
                    decode_message(user->language, rec_msg.payload, decoded); 
                    snprintf(message, message_size, "User %d,%s: %s\n", user->id, user->name, decoded);
                    

                    struct_multicast_message send;//for multicast
                    trans_message(&send, decoded);
                    // encode_to_binary(message, send.payload_bin );
                    // encode_to_hexadecimal(message, send.payload_hex);
                    // encode_to_octal(message, send.payload_oct );
                    // encode_to_ascii(message, send.payload_ascii );

                    int i ;
                    for (i=0; i < client_count; i++){
                    if (clients[i].id != 0){   
                    struct_message send_unicast;
                    send_unicast.id = clients[i].id;
                    send_unicast.type = 10;
                    strcpy(send_unicast.payload,"multicat message sent");
                    send_unicast.size_payload = sizeof(send_unicast.payload);
                    write(clients[i].socket,&send_unicast,sizeof(send_unicast));
                    }

                    }
                    sleep(0.5);
                    
                    if (sendto(mult.sockfd, &send, sizeof(send), 0, (struct sockaddr *)&mult.multicast_addr, sizeof(mult.multicast_addr)) < 0) {
                        perror("sendto");
                    }
                    

                    free(message);
                    break;
                    }
                    else{

                        printf("user autentication failed\n");
                        struct_message auteticate;
                        auteticate.id = user->id;
                        auteticate.laguage = user->language;
                        strcpy(auteticate.payload, "");
                        auteticate.type = 9;
                        auteticate.size_payload = 0;
                        write(user->socket,&auteticate,sizeof(auteticate));
                        sleep(2);

                        if (client_count != 1){
                            sleep(2);
                            close(user->socket);
                            size_t exit_message_size = snprintf(NULL, 0, "User %d,%s has logged out\n", user->id, user->name);
                            char *exit_message = malloc(exit_message_size);
                         if (exit_message == NULL) {
                              perror("malloc");
                              break; // Handle error
                          }
                         snprintf(exit_message, exit_message_size, "User %d,%s has logged out\n", user->id, user->name);
                            clients[user->id - 1].id = 0;

                         pthread_mutex_lock(&clients_mutex);
                         client_count--;
                         availible_language[user->language]--;

                         pthread_mutex_unlock(&clients_mutex);

                         struct_multicast_message exit_msg;
                         encode_to_binary(exit_message, exit_msg.payload_bin );
                            encode_to_hexadecimal(exit_message, exit_msg.payload_hex);
                            encode_to_octal(exit_message, exit_msg.payload_oct );
                            encode_to_ascii(exit_message, exit_msg.payload_ascii );

                         if (sendto(mult.sockfd, &exit_msg, sizeof(exit_msg), 0, (struct sockaddr *)&mult.multicast_addr, sizeof(mult.multicast_addr)) < 0) {
                             perror("sendto");
                         }
                         free(exit_message);
                         pthread_exit(0);
                            pthread_mutex_lock(&clients_mutex);
                                client_count--;
                                availible_language[user->language]--;

                            pthread_mutex_unlock(&clients_mutex);
                            clients[user->id - 1].id = 0;
                            close(user->socket);

                        }
                        else{
                        //running = 0;
                        close(user->socket);
                        //close(mult.sockfd);
                        //exit(0);
                        }

                    }





                case NO_LOG:

                    if (client_count != 1){
                            pthread_mutex_lock(&clients_mutex);
                                client_count--;
                                availible_language[user->language]--;
                            pthread_mutex_unlock(&clients_mutex);
                            clients[user->id - 1].id = 0;
                            close(user->socket);
                            break;

                    }
                    else{
                        //running = 0;
                        close(user->socket);
                        //close(mult.sockfd);
                       // exit(0);
                        break;
                        }
                    break;
                        


                         






                case LIST:
                if (rec_msg.id == user->id){
                    fflush(stdout);
                    struct_message send_list;
                    send_list.id = user->id;
                    send_list.laguage = user->language;
                    send_list.type = 5;
                    
                    int total_size;
                    char *list = create_user_list(&total_size);
                    strcpy(send_list.payload,list);
                    write(user->socket, &send_list, sizeof(send_list));
                    //send_message(list, user->socket);
                    free(list);
                    break;
                }else{

                        printf("user autentication failed\n");
                        struct_message auteticate;
                        auteticate.id = user->id;
                        auteticate.laguage = user->language;
                        strcpy(auteticate.payload, "");
                        auteticate.type = 9;
                        auteticate.size_payload = 0;
                        write(user->socket,&auteticate,sizeof(auteticate));
                        sleep(2);

                        if (client_count != 1){
                            sleep(2);
                            close(user->socket);
                            size_t exit_message_size = snprintf(NULL, 0, "User %d,%s has logged out\n", user->id, user->name);
                            char *exit_message = malloc(exit_message_size);
                         if (exit_message == NULL) {
                              perror("malloc");
                              break; // Handle error
                          }
                         snprintf(exit_message, exit_message_size, "User %d,%s has logged out\n", user->id, user->name);
                            clients[user->id - 1].id = 0;

                         pthread_mutex_lock(&clients_mutex);
                         client_count--;
                         availible_language[user->language]--;
                         pthread_mutex_unlock(&clients_mutex);

                         struct_multicast_message exit_msg;
                         encode_to_binary(exit_message, exit_msg.payload_bin );
                            encode_to_hexadecimal(exit_message, exit_msg.payload_hex);
                            encode_to_octal(exit_message, exit_msg.payload_oct );
                            encode_to_ascii(exit_message, exit_msg.payload_ascii );

                         if (sendto(mult.sockfd, &exit_msg, sizeof(exit_msg), 0, (struct sockaddr *)&mult.multicast_addr, sizeof(mult.multicast_addr)) < 0) {
                             perror("sendto");
                         }
                         free(exit_message);
                         pthread_exit(0);
                            pthread_mutex_lock(&clients_mutex);
                                client_count--;
                                availible_language[user->language]--;
                            pthread_mutex_unlock(&clients_mutex);
                            clients[user->id - 1].id = 0;
                            close(user->socket);

                        }
                        else{
                        //running = 0;
                        close(user->socket);
                        //close(mult.sockfd);
                       // exit(0);
                        }

                    }

                case EXIT:

                    
                    exit_ack.id = user->id;
                    exit_ack.laguage - user->language;
                    strcpy(exit_ack.payload, "");
                    exit_ack.size_payload = 0;
                    exit_ack.type = 8;
                    write(user->socket, &exit_ack, sizeof(exit_ack));
                    fflush(stdout);
                    if (client_count == 1){
                        //int enable = 0;
                        //printf("multicas fs:%d",mult.sockfd);
                        // if(setsockopt(mult.sockfd, IPPROTO_IP, IP_DROP_MEMBERSHIP,&enable, sizeof(enable))<0){
                        //   perror("Error leaving multicast group");
                        // }
                        //printf("close chat room\n");
                        //close(mult.sockfd);
                        //printf("close socket user\n");
                        //close(user->socket);
                        //exit(0);
                        //running = 0;
                        close(user->socket);
                        //close(mult.sockfd);
                        //exit(0);
                        //kill(main_t, SIGTSTP);

                    }

                    sleep(2);
                    close(user->socket);
                    size_t exit_message_size = snprintf(NULL, 0, "User %d,%s has logged out\n", user->id, user->name);
                    char *exit_message = malloc(exit_message_size);
                    if (exit_message == NULL) {
                        perror("malloc");
                        break; // Handle error
                    }
                    snprintf(exit_message, exit_message_size, "User %d,%s has logged out\n", user->id, user->name);
                    clients[user->id - 1].id = 0;

                    pthread_mutex_lock(&clients_mutex);
                    client_count--;
                    availible_language[user->language]--;
                    pthread_mutex_unlock(&clients_mutex);

                    struct_multicast_message exit_msg;
                    trans_message(&exit_msg, exit_message);
                    // encode_to_binary(exit_message, exit_msg.payload_bin );
                    // encode_to_hexadecimal(exit_message, exit_msg.payload_hex);
                    // encode_to_octal(exit_message, exit_msg.payload_oct );
                    // encode_to_ascii(exit_message, exit_msg.payload_ascii );

                    if (sendto(mult.sockfd, &exit_msg, sizeof(exit_msg), 0, (struct sockaddr *)&mult.multicast_addr, sizeof(mult.multicast_addr)) < 0) {
                        perror("sendto");
                    }
                    free(exit_message);
                    pthread_exit(0);
                case KEEP_ALIVE:
                    
                        ack.id = user->id;
                        ack.laguage = user->language;
                        strcpy(ack.payload, "");
                        ack.type =11;
                        ack.size_payload = 0;
                        write(user->socket,&ack,sizeof(ack));
                    printf("Keep alive\n");
                    break;
            } 
            //free(rec_msg);

            free(buffer); // Free the allocated buffer
        }
    }

    // Handle client disconnection and cleanup
    close(user->socket);
    remove_client(user->socket);
    free(user);
    pthread_exit(NULL);
}



void close_server(void *arg) {
    fd_set rfds;
    struct timeval tv;
    int retval;

    // Watch stdin (fd 0) to see when it has input.
    FD_ZERO(&rfds);
    FD_SET(0, &rfds);

    // Infinite loop to wait for user to press Enter
    while (1) {
        // Set the timeout to NULL to wait indefinitely for input
        retval = select(1, &rfds, NULL, NULL, NULL);
        if (retval == -1) {
            perror("select()");
            exit(EXIT_FAILURE);
        } else if (retval) {
            // Check if stdin (fd 0) is set in rfds
            if (FD_ISSET(0, &rfds)) {
                // Read the input to clear it from stdin
                char buffer[2];  // Buffer to hold '\n' and '\0'
                fgets(buffer, sizeof(buffer), stdin);
                
                // Perform server shutdown actions here
                running_multicast = 0;
                close(mult.sockfd);
                for (int i = 0; i < client_count; i++) {
                    close(clients[i].socket);
                }
                close(server_socket);
                
                printf("Server closed.\n");
                exit(0);
                break; // Exit loop after server shutdown
            }
        } else {
            // This branch typically won't execute in an infinite loop scenario
            printf("Timeout occurred. Press enter to continue...\n");
        }
    }
}



int main() {
    main_t = getpid();
    // Set up signal handler for SIGTSTP
    signal(SIGTSTP, handle_sigtstp);
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    pthread_t tid;


    server_socket = socket(AF_INET, SOCK_STREAM , 0);
    int enable = 1;

    if (server_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &enable, sizeof(enable))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
   

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, MAX_CLIENTS) == -1) {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);
    int next_id =0;
    while (running_multicast) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_len);
        printf("Connection accepted\n");
        if (client_socket == -1) {
            perror("Accept failed");
            continue;
        }
        int i;
        for(i=0; i< client_count; i++){
            if (!clients[i].id ){
                next_id = i ;
                break;
            }
        }
        client_count++;
        User *new_user = (User *)malloc(sizeof(User));
        new_user->socket = client_socket;
        new_user->id = ++next_id;
        
        //snprintf(new_user->name, sizeof(new_user->name), "User%d", new_user->id);
        //snprintf(new_user->language, sizeof(new_user->language), 1);

        pthread_mutex_lock(&clients_mutex);
        clients[next_id - 1] = *new_user;
        pthread_mutex_unlock(&clients_mutex);

      

        if (pthread_create(&tid, NULL, handle_client, (void *)new_user) != 0) {
            perror("Thread creation failed");
            free(new_user);
        }

        if (pthread_create(&tid, NULL, close_server, (void *)new_user) != 0) {
            perror("Thread creation failed");
            free(new_user);
        }

        pthread_detach(tid);
    }

    close(server_socket);
    return 0;
}