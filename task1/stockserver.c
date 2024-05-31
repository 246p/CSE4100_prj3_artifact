#include "csapp.h"

typedef struct{
	int maxfd;
	fd_set read_set;
	fd_set ready_set;
	int nready;
	int maxi;
	int clientfd[FD_SETSIZE];
	rio_t clientrio[FD_SETSIZE];
} pool;

typedef struct item{
    int ID;
    int left_stock;
    int price;
    int readcnt;
    sem_t mutex;
    struct item *left;
    struct item *right;
}item;
void job(char * buf, int connfd);
void init_pool(int listenfd, pool *p);
void add_client(int connfd, pool *p);
void check_clients(pool *p);
void init_stock(void);
void save_stock(void);
void save_inorder(item *root, FILE *fp);
item* stock=NULL;
item* create(int ID, int left, int price);
item* insert(item* root, int ID, int left, int price);
item* search(item* root, int ID);

void inorder(item* root, char* send);

void sigint_handler(int sig);


int main(int argc, char **argv) {
    signal(SIGINT, (void *)sigint_handler);
    int listenfd, connfd;
    socklen_t clientlen;
    char client_hostname[MAXLINE], client_port[MAXLINE];
    struct sockaddr_storage clientaddr;  
    static pool pool;
    if (argc != 2){
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }
    init_stock();
    listenfd = Open_listenfd(argv[1]);
    init_pool(listenfd, &pool);
    while (1){
        pool.ready_set=pool.read_set;
        pool.nready=Select(pool.maxfd+1, &pool.ready_set, NULL, NULL, NULL);
		if(FD_ISSET(listenfd, &pool.ready_set)){
            clientlen = sizeof(struct sockaddr_storage); 
			connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        	Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
        	printf("Connected to (%s, %s)\n", client_hostname, client_port);
			add_client(connfd, &pool);
		}
        check_clients(&pool);
    }
    exit(0);
}

void init_stock(void) {
    int id, left, price;
    FILE *fp = fopen("stock.txt", "r");
    if (fp == NULL) {
        app_error("no stock.txt file");
        exit(1);
    }
    while (fscanf(fp, "%d %d %d", &id, &left, &price) == 3)
        stock = insert(stock, id, left, price);
    fclose(fp);
}

item* create(int ID, int left, int price) {
    item *new_node = (item *)malloc(sizeof(item));
    if (new_node == NULL) {
        app_error("malloc error");
        exit(1);
    }
    new_node->ID = ID;
    new_node->left_stock = left;
    new_node->price = price;
    new_node->readcnt = 0;
    if (sem_init(&new_node->mutex, 0, 1) == -1) {
        app_error("sem_init error");
        free(new_node);
        exit(1);
    }
    new_node->left = NULL;
    new_node->right = NULL;
    return new_node;
}


void save_stock(void) {
    FILE *fp = fopen("stock.txt", "w");
    if (fp == NULL) {
        app_error("writing stock.txt error");
        exit(1);
    }
    save_inorder(stock, fp);
    fclose(fp);
}

void save_inorder(item *root, FILE *fp) {
    if (root != NULL) {
        save_inorder(root->left, fp);
        fprintf(fp, "%d %d %d\n", root->ID, root->left_stock, root->price);
        save_inorder(root->right, fp);
    }
}
item* insert(item* root, int ID, int left, int price) {
    if (root == NULL) return create(ID, left, price);
    if (ID < root->ID) root->left = insert(root->left, ID, left, price);
    else if (ID > root->ID) root->right = insert(root->right, ID, left, price);
    return root;
}

item* search(item* root, int ID) {
    if (root == NULL || root->ID == ID) return root;
    if (ID < root->ID) return search(root->left, ID);
    else return search(root->right, ID);
}

void inorder(item* root, char* send) {
    char buf[MAXLINE]={0};
    if (root != NULL) {
        inorder(root->left, send);
        sprintf(buf,"%d %d %d\n", root->ID, root->left_stock, root->price);
        strcat(send,buf);
        inorder(root->right, send);
    }
}


void init_pool(int listenfd, pool *p) {
    int i;
    p->maxi = -1;
    for (i=0; i< FD_SETSIZE; i++)  
	p->clientfd[i] = -1;
    p->maxfd = listenfd;
    FD_ZERO(&p->read_set);
    FD_SET(listenfd, &p->read_set); 
}

void add_client(int connfd, pool *p){
    int i;
    p->nready--;
    for (i = 0; i < FD_SETSIZE; i++)
        if (p->clientfd[i] < 0){
            p->clientfd[i] = connfd;
            Rio_readinitb(&p->clientrio[i], connfd);
            FD_SET(connfd, &p->read_set);
            if (connfd > p->maxfd) p->maxfd = connfd;
            if (i > p->maxi) p->maxi = i;
            break;
        }
    if (i == FD_SETSIZE)
        app_error("add_client error: Too many clients");
}

void check_clients(pool *p) 
{
    int i, connfd, n;
    char buf[MAXLINE]; 
    rio_t rio;

    for (i = 0; (i <= p->maxi) && (p->nready > 0); i++) {
        connfd = p->clientfd[i];
        rio = p->clientrio[i];
        if ((connfd > 0) && (FD_ISSET(connfd, &p->ready_set))){
            p->nready--;
            if ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0){
                //printf("current connection : %d ", client_cnt);
                printf("Server received %d bytes on fd %d\n", n, connfd);
                job(buf, connfd);
            }
            else{
                Close(connfd);
                FD_CLR(connfd, &p->read_set);
                p->clientfd[i] = -1;
            }
        }
    }
}

void job(char * buf, int connfd){
    char send[MAXLINE] = {0};
    int id, amount;
    item *target;
    if (!strncmp(buf, "exit", 4)){
        sprintf(send, "exit\n");
        Rio_writen(connfd, send, MAXLINE);
    }
    else if (!strncmp(buf, "show", 4)){
        inorder(stock,send);
        Rio_writen(connfd, send, MAXLINE);
    }
    else if (!strncmp(buf, "buy", 3)){
        sscanf(buf + 4, "%d %d", &id, &amount);
        target = search(stock, id);
        if (target->left_stock >= amount){
            target->left_stock -= amount;
            sprintf(send, "[buy] success\n");
        }
        else sprintf(send, "Not enough left stock\n");
        Rio_writen(connfd, send, MAXLINE);
    }
    else if (!strncmp(buf, "sell", 4)){
        sscanf(buf + 5, "%d %d", &id, &amount);
        target = search(stock, id);
        target->left_stock += amount;
        sprintf(send, "[sell] success\n");
        Rio_writen(connfd, send, MAXLINE);
    }
}


void sigint_handler(int sig){
    int old_errno = errno;
    sigset_t set, oldset;
    Sigfillset(&set);
    Sigprocmask(0, &set, &oldset);
    save_stock();
    Sigprocmask(2, &oldset, NULL);
    errno = old_errno;
    _exit(0);
}