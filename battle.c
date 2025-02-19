#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#ifndef PORT
    #define PORT 59694
#endif

#define MAX_SCORE 30
#define MIN_SCORE 20
#define MAX_ATTACK_TIME 30
#define MAX_MESSAGES 5

struct game {
    struct client *opponent;
    int past_fd;
    int in_game;
    int hitpoints;
    int powermoves;
};

struct client {
    struct client *next;
    struct all_messages msg;
    struct game battle;
    int messages_sent;
    time_t turn_start_time;
    int fd;
    int processing;
    int turn;
    char name[256];
    struct in_addr ipaddr;
};

struct all_messages {
    char message_buffer[256];
    char cmd_buffer[60];
    int cmd_rm;
    char *command_end;
    int command_buff;
    int gameroom;
    char *end;
    int buff;
};


struct client *addclient(struct client *top, int socket_fd, struct in_addr ip_address);
struct client *removeclient(struct client *top, int socket_fd);
void broadcast(struct client *top, char *s, int size);

int process_speak_result(struct client *player);
int process_combat(struct client *player, struct client *top);
int process_disconnect(struct client *player);
int process_awaiting_opponent(struct client *player);
int process_joined_arena(struct client *player, struct client *top);
int handleclient(struct client *player, struct client *top);

int generatehitpoints();
int locate_network_linebreak(char *buffer, int buffer_length);

int write_to_opponent(struct client *client_data, char *buffer, int newline_pos);
int process_buffer(struct client *client_data, int newline_pos);
int read_message(struct client *client_data);

int search_for_opponent(struct client *head, struct client *current_client);

int write_stats(int fd, char *name, int hitpoints, int powermoves);
int display_stats(struct client *client1, struct client *client2);

int write_options(int fd, struct client *client, struct client *opponent);
int display_options(struct client *client1, struct client *client2);

int generate_powermoves();
int bindandlisten(void);

int read_from_client(struct client *client_data);
void reset_message_buffer(struct client *client_data);
int read_and_discard(struct client *client_data);

int calculate_damage(struct client *attacker, struct client *defender, int attack_kind);
int send_message(int fd, char *message);
int handle_defeat(struct client *winner, struct client *loser, struct client *top);
int the_damage(struct client *top, struct client *p1, struct client *p2, int attack_type);

int read_command(struct client *player, struct client *top);
int process_attack(struct client *player, struct client *top, int attack_type);
int process_speak(struct client *player, char *outbuf);
int process_command(struct client *player, struct client *top);


int locate_command(char *input_buf, int buf_len, int remaining_powermoves);

int standard_attack();

int main(void)
{
    printf("%d\n", PORT);
    int clientfd, maxfd, nready;
    struct client *p; 
    struct client *head = NULL; 
    socklen_t len;
    struct sockaddr_in q;
    fd_set allset;
    fd_set rset;
    int i;

 
    int listenfd = bindandlisten();

    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);

    maxfd = listenfd;

    while (1)
    {
        rset = allset;

        nready = select(maxfd + 1, &rset, NULL, NULL, NULL);
        printf("%d\n", nready);
        if (nready == 0) {
            continue;
         }

        if (nready == -1) {
            perror("select");
            continue;
        }

        if(FD_ISSET(listenfd, &rset)){
            
            len = sizeof(q);
            if ((clientfd = accept(listenfd, (struct sockaddr *)&q, &len)) < 0) {
                perror("accept");
                exit(1);
            }
            FD_SET(clientfd, &allset);
            if (clientfd > maxfd) {
                maxfd = clientfd;
            }
            printf("connection from %s\n", inet_ntoa(q.sin_addr));
            head = addclient(head, clientfd, q.sin_addr);


        }

        for(i = 0; i <= maxfd; i++)
        {
            if (FD_ISSET(i, &rset)) {
                for (p = head; p != NULL; p = p->next)
                {
                    if (p->fd == i)
                    {
                        int result = handleclient(p, head);
                        if (result == -1)
                        {
                            int tmp_fd = p->fd;
                            head = removeclient(head, p->fd);
                            FD_CLR(tmp_fd, &allset);
                            close(tmp_fd);
                        }
                        break;
                    }
                }
            }
        }
    }
    return 0;
}

int read_message(struct client *client_data)
{
    int newline_pos;
    int initial_loop = 0;
    int bytes_read;
    int func_result = 0;

    while (1)
    {
        bytes_read = read(client_data->fd, client_data->msg.end, client_data->msg.gameroom);
        if (bytes_read == -1)
        {
            perror("read");
            return -1;
        }

        if (bytes_read == 0)
        {
            break;
        }

        initial_loop = 1;
        client_data->msg.buff += bytes_read;
        newline_pos = locate_network_linebreak(client_data->msg.message_buffer, client_data->msg.buff);

        if (newline_pos >= 0)
        {
            client_data->msg.message_buffer[newline_pos] = '\n';
            client_data->msg.message_buffer[newline_pos + 1] = '\0';

            func_result = process_buffer(client_data, newline_pos);
            if (func_result == -1) {
                return -1;
            }

            client_data->msg.buff -= newline_pos + 2;
            memmove(client_data->msg.message_buffer, client_data->msg.message_buffer + newline_pos + 2, sizeof(client_data->msg.message_buffer));
        }
        client_data->msg.gameroom = sizeof(client_data->msg.message_buffer) - client_data->msg.buff;
        client_data->msg.end = client_data->msg.message_buffer + client_data->msg.buff;

        if (func_result == 1 || func_result == 2)
        {
            break;
        }

        if (client_data->msg.gameroom == 0 && newline_pos < 0)
        {
            return -1;
        }

        break;
    }

    if (initial_loop == 0)
    {
        return -1;
    }

    return func_result;
}


int write_to_opponent(struct client *client_data, char *buffer, int newline_pos) {
    int write_status;
    client_data->messages_sent++;
    sprintf(buffer, "%s takes a break to tell you: \r\n", client_data->name);
    write_status = write(client_data->battle.opponent->fd, buffer, strlen(buffer) + 1);
    if (write_status == -1)
    {
        perror("write");
        return -1;
    }
    strncpy(buffer, client_data->msg.message_buffer, strlen(client_data->msg.message_buffer));
    write_status = write(client_data->battle.opponent->fd, buffer, strlen(buffer) + 1);
    if (write_status == -1)
    {
        perror("write");
        return -1;
    }
    return 2;
}


int write_stats(int fd, char *name, int hitpoints, int powermoves) {
    char buffer[512];
    int write_status;

    sprintf(buffer, "Your hitpoints: %d\r\n", hitpoints);
    write_status = write(fd, buffer, strlen(buffer) + 1);
    if (write_status == -1)
    {
        perror("write");
        return -1;
    }

    sprintf(buffer, "Your powermoves: %d\n\r\n", powermoves);
    write_status = write(fd, buffer, strlen(buffer) + 1);
    if (write_status == -1)
    {
        perror("write");
        return -1;
    }

    sprintf(buffer, "%s's hitpoints: %d\r\n", name, hitpoints);
    write_status = write(fd, buffer, strlen(buffer) + 1);
    if (write_status == -1)
    {
        perror("write");
        return -1;
    }

    return 0;
}


int process_buffer(struct client *client_data, int newline_pos) {
    char buffer[512];
    if (client_data->name[0] == '\0')
    {
        client_data->msg.message_buffer[newline_pos] = '\0';
        strncpy(client_data->name, client_data->msg.message_buffer, sizeof(client_data->msg.message_buffer));
        return 1;
    }
    else
    {
        return write_to_opponent(client_data, buffer, newline_pos);
    }
}






int display_stats(struct client *client1, struct client *client2) {
    if (write_stats(client1->fd, client2->name, client1->battle.hitpoints, client1->battle.powermoves) == -1) {
        return -1;
    }
    if (write_stats(client2->fd, client1->name, client2->battle.hitpoints, client2->battle.powermoves) == -1) {
        return -1;
    }
    return 0;
}





int standard_attack()
{
    return (random() % 5) + 2;
}

int locate_command(char *input_buf, int buf_len, int remaining_powermoves)
{
    int index = 0;
    while ((input_buf[index] !='\0') && (index < buf_len))
    {
        if ((input_buf[index] == 'a') || (input_buf[index] == 'p' && remaining_powermoves > 0) || (input_buf[index] == 's'))
        {
            return index;
        } 
        index++;   
    }
    return -1;
}


int write_options(int fd, struct client *client, struct client *opponent) {
    char buffer[512];
    int write_status;

    sprintf(buffer, "\n(a)ttack\r\n");
    write_status = write(fd, buffer, strlen(buffer) + 1);
    if (write_status == -1)
    {
        perror("write");
        return -1;
    }

    if (client->battle.powermoves > 0)
    {
        sprintf(buffer, "(p)owermove\r\n");
        write_status = write(fd, buffer, strlen(buffer) + 1);
        if (write_status == -1)
        {
            perror("write");
            return -1;
        }
    }

    sprintf(buffer, "(s)peak something\r\n");
    write_status = write(fd, buffer, strlen(buffer) + 1);
    if (write_status == -1)
    {
        perror("write");
        return -1;
    }

    sprintf(buffer, "waiting for %s to strike...\n\r\n", opponent->name);
    write_status = write(opponent->fd, buffer, strlen(buffer) + 1);
    if (write_status == -1)
    {
        perror("write");
        return -1;
    }

    return 0;
}

int handle_defeat(struct client *winner, struct client *loser, struct client *top)
{
    char outbuf[512];
    sprintf(outbuf, "%s gives up. You win!\r\n", loser->name);
    if (send_message(winner->fd, outbuf) == -1) return -1;
    sprintf(outbuf, "You are no match for %s. You scurry away...\r\n", winner->name);
    if (send_message(loser->fd, outbuf) == -1) return -1;
    (winner->battle).in_game = 0;
    (loser->battle).in_game = 0;
    sprintf(outbuf, "\nAwaiting next opponent...\r\n");
    if (send_message(winner->fd, outbuf) == -1) return -1;
    if (send_message(loser->fd, outbuf) == -1) return -1;
    if (search_for_opponent(top, winner) == 1)
    {
        display_stats(winner, (winner->battle).opponent);
        display_options(winner, (winner->battle).opponent);
    }
    if (search_for_opponent(top, loser) == 1)
    {
        display_stats(loser, (loser->battle).opponent);
        display_options(loser, (loser->battle).opponent);
    }
    return 0;
}

int calculate_damage(struct client *attacker, struct client *defender, int attack_kind)
{
    int damage = standard_attack();
    if (attack_kind != 0)
    {
        int accuracy = (random() % 2);
        if (accuracy == 1)
        {
            damage = 3 * damage;
        }
        else
        {
            damage = 0;
        }
        (attacker->battle).powermoves = (attacker->battle).powermoves - 1;
    }
    return damage;
}

int send_message(int fd, char *message)
{
    int errr = write(fd, message, strlen(message) + 1);
    if (errr == -1)
    {
        perror("write");
        return -1;
    }
    return 0;
}

int display_options(struct client *client1, struct client *client2) {
    if (client1->turn == 1)
    {
        return write_options(client1->fd, client1, client2);
    }
    else
    {
        return write_options(client2->fd, client2, client1);
    }
}




int the_damage(struct client *top, struct client *p1, struct client *p2, int attack_type)
{
    char outbuf[512];
    p1->turn = 0;
    int damage = calculate_damage(p1, p2, attack_type);
    if (damage > 0)
    {
        sprintf(outbuf, "\nYou hit %s for %d damage!\r\n", p2->name, damage);
        if (send_message(p1->fd, outbuf) == -1) return -1;
        sprintf(outbuf, "\n%s hits you for %d damage!\r\n", p1->name, damage);
        if (send_message(p2->fd, outbuf) == -1) return -1;
        (p2->battle).hitpoints = (p2->battle).hitpoints - damage;
        if ((p2->battle).hitpoints <= 0)
        {
            return handle_defeat(p1, p2, top);
        }
    }
    else
    {
        sprintf(outbuf, "\nYou missed!\r\n");
        if (send_message(p1->fd, outbuf) == -1) return -1;
        sprintf(outbuf, "\n%s missed you!\r\n", p1->name);
        if (send_message(p2->fd, outbuf) == -1) return -1;
    }
    p2->turn = 1;
    display_stats(p1, p2);
    display_options(p1, p2);
    return 0;
}




int process_speak(struct client *player, char *outbuf) {
    int errr;
    (player->msg).buff = 0;
    (player->msg).gameroom = sizeof((player->msg).message_buffer);
    (player->msg).end = (player->msg).message_buffer;
    sprintf(outbuf, "\nSpeak: ");
    errr = write(player->fd, outbuf, strlen(outbuf) + 1);
    if (errr == -1) {
        perror("write");
        return -1;
    }
    player->processing = 1;
    return 0;
}

int process_command(struct client *player, struct client *top) {
    int find;
    int the_first = 0;
    int nbytes;
    char outbuf[512];

    while (1) {
        nbytes = read_command(player, top);
        if (nbytes <= 0) {
            break;
        }
        the_first = 1;
        (player->msg).command_buff = (player->msg).command_buff + nbytes;
        find = locate_command((player->msg).cmd_buffer, (player->msg).command_buff, (player->battle).powermoves);

        if (find >= 0) {
            if ((player->msg).cmd_buffer[find] == 'a') {   
                return process_attack(player, top, 0);
            } else if ((player->msg).cmd_buffer[find] == 'p' && (player->battle).powermoves > 0) {
                return process_attack(player, top, 1);
            } else {   
                if (player->messages_sent >= 5){
                    sprintf(outbuf, "\nMessage limit (5) reached. A regular attack is forced.\n");
                    int errr = write(player->fd, outbuf, strlen(outbuf) + 1);
                    if (errr == -1) {
                        perror("write");
                        return -1;
                    }
                    return process_attack(player, top, 0);
                } else{
                    if (process_speak(player, outbuf) == -1) {
                        return -1;
                    }
                }
            }
            (player->msg).command_buff = 0;
            (player->msg).cmd_rm = sizeof((player->msg).cmd_buffer);
            (player->msg).command_end = (player->msg).cmd_buffer;
        }

        if ((player->msg).cmd_rm == 0 && find == -1) {
            return -1;
        }
        time_t current_time = time(NULL);
        double elapsed_time = difftime(current_time, player->turn_start_time);
        if (elapsed_time > MAX_ATTACK_TIME) {
            return -1;
        }

        break;
    }

    if (the_first == 0) {
        return -1;
    }

    return 0;
}

int read_command(struct client *player, struct client *top) {
    int nbytes = read(player->fd, (player->msg).command_end, (player->msg).cmd_rm);
    if (nbytes == -1) {
        perror("read");
        return -1;
    }
    return nbytes;
}

int process_attack(struct client *player, struct client *top, int attack_type) {
    player->messages_sent = 0;
    return the_damage(top, player, ((player->battle).opponent), attack_type);
}

int process_speak_result(struct client *player) {
    int speak_result = read_message(player);
    if (speak_result == 2) {
        player->processing = 0;
    }
    return speak_result;
}

int process_combat(struct client *player, struct client *top) {
    player->turn_start_time = time(NULL);
    return process_command(player, top);
}

int process_disconnect(struct client *player) {
    printf("Goodbye %s\n", inet_ntoa(player->ipaddr));
    return -1;
}

int process_awaiting_opponent(struct client *player) {
    int errr;
    char *message = "You are awaiting an opponent...\r\n";
    errr = write(player->fd, message, strlen(message) + 1);
    if (errr == -1) {
        perror("write");
        return -1;
    }
    return 0;
}

int process_joined_arena(struct client *player, struct client *top) {
    char outbuf[512];
    sprintf(outbuf, "\n**%s has joined the arena**\r\n", player->name);
    broadcast(top, outbuf, strlen(outbuf) + 1);
    return search_for_opponent(top, player);
}

int handleclient(struct client *player, struct client *top) {  
    if (player->processing == 1) {
        return process_speak_result(player);
    }
    if ((player->battle).in_game == 1 && player->turn == 1) {
        return process_combat(player, top);
    }
    if (player->turn == 0 && player->name[0] != '\0') {
        return read_and_discard(player);
    }

    int process_message_result = read_message(player);
    if (process_message_result == -1) {
        return process_disconnect(player);
    }

    if (process_message_result == 1) {   
        if (process_awaiting_opponent(player) == -1) {
            return -1;
        }
        int search_result = process_joined_arena(player, top);
        if (search_result == -1) {
            return search_result;
        }
        if (search_result == 1) {   
            display_stats(player, (player->battle).opponent);
            display_options(player, (player->battle).opponent);
        }
    }

    return process_message_result;
}


int read_from_client(struct client *client_data) {
    int num_bytes = read(client_data->fd, (client_data->msg).end, (client_data->msg).gameroom);
    if (num_bytes == -1) {
        perror("read");
        return -1;
    }
    return num_bytes;
}

void reset_message_buffer(struct client *client_data) {
    (client_data->msg).gameroom = sizeof((client_data->msg).message_buffer);
    (client_data->msg).end = (client_data->msg).message_buffer;
    (client_data->msg).buff = 0;
}

int read_and_discard(struct client *client_data) {
    int first_run = 0;
    int num_bytes; 

    while (1) {
        num_bytes = read_from_client(client_data);
        if (num_bytes <= 0) {
            break;
        }
        first_run = 1;
        (client_data->msg).buff = (client_data->msg).buff + num_bytes;

        (client_data->msg).gameroom = sizeof((client_data->msg).message_buffer) - (client_data->msg).buff;
        if ((client_data->msg).gameroom > 0) {
            reset_message_buffer(client_data);
            break;
        }
        reset_message_buffer(client_data);
    }   
    if (first_run == 0) {
        return -1;
    }

    return 0;
}


int search_for_opponent(struct client *head, struct client *current_client)
{   
    char engagement_message[300];
    struct client *temp_client;
    for (temp_client = head; temp_client != NULL; temp_client = temp_client->next)
    {
        if ((temp_client->battle).in_game == 0 && (((temp_client->battle).past_fd != current_client->fd) || ((current_client->battle).past_fd != temp_client->fd)) && (temp_client->name)[0] != '\0' && temp_client->fd != current_client->fd)
        {   
            int write_status;
            (temp_client->battle).in_game = 1;
            (current_client->battle).in_game = 1;
            (temp_client->battle).opponent = current_client;
            (current_client->battle).opponent = temp_client;
            (temp_client->battle).past_fd = current_client->fd;
            (current_client->battle).past_fd = temp_client->fd;
            (temp_client->battle).hitpoints = generatehitpoints();
            (current_client->battle).hitpoints = generatehitpoints();
            (temp_client->battle).powermoves = generate_powermoves();
            (current_client->battle).powermoves = generate_powermoves();
            current_client->turn = 1;
            sprintf(engagement_message, "You engage %s\r\n", current_client->name);
            write_status = write(temp_client->fd, engagement_message, strlen(engagement_message) + 1);
            if (write_status == -1)
            {
                perror("write");
                return -1;
            }
            sprintf(engagement_message, "You engage %s\r\n", temp_client->name);
            write_status = write(current_client->fd, engagement_message, strlen(engagement_message) + 1);
            if (write_status == -1)
            {
                perror("write");
                return -1;
            }
            return 1;
        }
    }
    return 0;
}

int bindandlisten(void) {
    struct sockaddr_in r;
    int listenfd;
    // set up sockets
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }
    int yes = 1;
    if ((setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) == -1) {
        perror("setsockopt");
    }
    memset(&r, '\0', sizeof(r));
    r.sin_family = AF_INET;
    r.sin_addr.s_addr = INADDR_ANY;
    r.sin_port = htons(PORT);

    if (bind(listenfd, (struct sockaddr *)&r, sizeof(r))) {
        perror("bind");
        exit(1);
    }

    if (listen(listenfd, SOMAXCONN)) {
        perror("listen");
        exit(1);
    }
    return listenfd;  //Return new socket.
}


struct client *addclient(struct client *top, int socket_fd, struct in_addr ip_address) {
    struct client *new_client = malloc(sizeof(struct client));
    struct client *temp_client = top;
    if (!new_client) {
        perror("malloc");
        exit(1);
    }
    new_client->processing = 0;

    (new_client->msg).buff = 0;
    (new_client->msg).end = (new_client->msg).message_buffer;
    (new_client->msg).gameroom = sizeof((new_client->msg).message_buffer);

    (new_client->msg).command_buff = 0;
    (new_client->msg).command_end = (new_client->msg).cmd_buffer;
    (new_client->msg).cmd_rm = sizeof((new_client->msg).cmd_buffer);
    new_client->fd = socket_fd;
    new_client->turn = 0;
    new_client->ipaddr = ip_address;
    new_client->next = NULL;
    (new_client->name)[0] = '\0';
    (new_client->battle).opponent = NULL;
    (new_client->battle).past_fd = new_client->fd;
    (new_client->battle).in_game = 0;
    (new_client->battle).hitpoints = 0;
    (new_client->battle).powermoves = 0;

    const char *welcome_message = "What is your name? \r\n";
    int write_status = write(socket_fd, welcome_message, strlen(welcome_message) + 1);
    if (write_status != strlen(welcome_message) + 1)
    {
        perror("write(addclient)");
        exit(EXIT_FAILURE);
    }

    if (top == NULL)
    {
        top = new_client;
        return top;
    }
    
    while (temp_client->next != NULL)
    {
        temp_client = temp_client->next;
    }
    temp_client->next = new_client;
    return top;
}


struct client *removeclient(struct client *top, int socket_fd) {
    struct client **client_ptr;

    for (client_ptr = &top; *client_ptr && (*client_ptr)->fd != socket_fd; client_ptr = &(*client_ptr)->next)
        ;
    if (*client_ptr) {
        char buffer[512];
        if (((*client_ptr)->battle).in_game == 1)
        {   
            int write_status;
            sprintf(buffer, "\n--%s dropped. You win!\n\r\n", (*client_ptr)->name);
            write_status = write((((*client_ptr)->battle).opponent)->fd, buffer, strlen(buffer) + 1);
            if (write_status == -1)
            {
                perror("write");
                exit(1);
            }
            (((*client_ptr)->battle).opponent)->turn = 0;
            ((((*client_ptr)->battle).opponent)->battle).in_game = 0;
            ((((*client_ptr)->battle).opponent)->battle).past_fd = 0;
            sprintf(buffer, "\nAwaiting next opponent...\r\n");
            write_status = write((((*client_ptr)->battle).opponent)->fd, buffer, strlen(buffer) + 1);
            if (write_status == -1)
            {
                perror("write");
                exit(1);
            }
            int search_result = search_for_opponent(top, ((((*client_ptr)->battle).opponent)));
            if (search_result == 1)
            {
                display_stats(((*client_ptr)->battle).opponent, ( (((*client_ptr)->battle).opponent)->battle).opponent);
                display_options(((*client_ptr)->battle).opponent, ( (((*client_ptr)->battle).opponent)->battle).opponent);
            }
        }
        sprintf(buffer, "\n**%s leaves **\r\n", (*client_ptr)->name);
        broadcast(top, buffer, strlen(buffer) + 1);
        struct client *temp = (*client_ptr)->next;
        printf("Removing client %d %s\n", socket_fd, inet_ntoa((*client_ptr)->ipaddr));
        free(*client_ptr);
        *client_ptr = temp;
    } else {
        fprintf(stderr, "Failed to remove fd %d\n",
                 socket_fd);
    }
    return top;
}


void broadcast(struct client *top, char *s, int size) {
    struct client *p;
    int check;
    for (p = top; p; p = p->next) {
        check = write(p->fd, s, size);
        if (check == -1){
            perror("write");
        }
    }
}


int locate_network_linebreak(char *buffer, int buffer_length) 
{
  int index = 0;

  while ((buffer[index] != '\0') && (index < buffer_length))
  {
    if (buffer[index] == '\r')
    {
      if (buffer[index + 1] == '\n')
      {
        return index;
      }
    }
    index++;
  }
  return -1;
}

int generatehitpoints(void)
{

    int randomnum = random() % (MAX_SCORE - MIN_SCORE + 1) + MIN_SCORE;
    return randomnum;
}

int generate_powermoves(void)
{

    int num = (random() % 3) + 1;
    return num;
}