#include "csapp.h"

#define SBUF 2000
#define MAXWORK 200
typedef struct{
	int maxfd;
	fd_set read_set;
	fd_set ready_set;
	int nready;
	int maxi;
	int clientfd[FD_SETSIZE];
	rio_t clientrio[FD_SETSIZE];
} pool;

typedef struct item {
    int ID;
    int left_stock;
    int price;
    int readcnt;
    sem_t mutex;
    struct item *left;
    struct item *right;
} item;


typedef struct {
    int *buf;       
    int n;
    int front;
    int rear;
    sem_t mutex;
    sem_t slots;
    sem_t items;
} sbuf_t;

void sbuf_init(sbuf_t *sp, int n);
void sbuf_deinit(sbuf_t *sp);
void sbuf_insert(sbuf_t *sp, int item);
int sbuf_remove(sbuf_t *sp);

void job(char *buf, int connfd);
void init_stock(void);
void save_stock(void);
void save_inorder(item *root, FILE *fp);
void destroy_tree(item *root);

item* stock = NULL;
item* create(int ID, int left, int price);
item* insert(item* root, int ID, int left, int price);
item* search(item* root, int ID);
void inorder(item* root, char* send);

void sigint_handler(int sig);
void *thread(void *vargp);
void con(int connfd);

sbuf_t sbuf;
int main(int argc, char **argv) {
    signal(SIGINT, (void *)sigint_handler);
    int listenfd, connfd;
    socklen_t clientlen = sizeof(struct sockaddr_in);
    struct sockaddr_in clientaddr;
    char client_hostname[MAXLINE], client_port[MAXLINE];
    pthread_t tid;
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }
    sbuf_init(&sbuf,SBUF);
    init_stock();
    listenfd = Open_listenfd(argv[1]);

    for (int i = 0; i < MAXWORK; i++)
	    Pthread_create(&tid, NULL, thread, NULL);
    
    while (1) { 
        clientlen = sizeof(struct sockaddr_storage);
	    connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen);
        Getnameinfo((SA *)&clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
        printf("Connected to (%s, %s)\n", client_hostname, client_port);
        sbuf_insert(&sbuf, connfd);
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
    char buf[MAXLINE] = {0};
    if (root != NULL) {
        inorder(root->left, send);
        sprintf(buf, "%d %d %d\n", root->ID, root->left_stock, root->price);
        strcat(send, buf);
        inorder(root->right, send);
    }
}

void job(char *buf, int connfd) {
    char send[MAXLINE] = {0};
    int id, amount;
    item *target;
    if (!strncmp(buf, "exit", 4)) {
        sprintf(send, "exit\n");
        Rio_writen(connfd, send, MAXLINE);
    }
    else if (!strncmp(buf, "show", 4)) {
        inorder(stock, send);
        Rio_writen(connfd, send, MAXLINE);
    }
    else if (!strncmp(buf, "buy", 3)) {
        sscanf(buf + 4, "%d %d", &id, &amount);
        target = search(stock, id);
        P(&target->mutex);
        if (target->left_stock >= amount) {
            target->left_stock -= amount;
            sprintf(send, "[buy] success\n");
        }
        else
            sprintf(send, "Not enough left stock\n");
        V(&target->mutex);
        Rio_writen(connfd, send, MAXLINE);
    }
    else if (!strncmp(buf, "sell", 4)) {
        sscanf(buf + 5, "%d %d", &id, &amount);
        target = search(stock, id);
        P(&target->mutex);
        target->left_stock += amount;
        sprintf(send, "[sell] success\n");
        V(&target->mutex);
        Rio_writen(connfd, send, MAXLINE);
    }
}

void *thread(void *vargp) {
    Pthread_detach(pthread_self());
    while(1){
        int connfd = sbuf_remove(&sbuf);
        con(connfd);
        Close(connfd); 
    }
}

void con(int connfd) {
    int n;
    char buf[MAXLINE];
    rio_t rio;
    Rio_readinitb(&rio, connfd);
    while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
        printf("Server received %d bytes on fd %d\n", n, connfd);
        job(buf, connfd);
    }
}

void sbuf_init(sbuf_t *sp, int n)
{
    sp->buf = Calloc(n, sizeof(int)); 
    sp->n = n;
    sp->front = sp->rear = 0;
    Sem_init(&sp->mutex, 0, 1);
    Sem_init(&sp->slots, 0, n);
    Sem_init(&sp->items, 0, 0);
}

void sbuf_deinit(sbuf_t *sp)
{
    Free(sp->buf);
}

void sbuf_insert(sbuf_t *sp, int item)
{
    P(&sp->slots);
    P(&sp->mutex);
    sp->buf[(++sp->rear)%(sp->n)] = item;
    V(&sp->mutex);
    V(&sp->items);
}

int sbuf_remove(sbuf_t *sp)
{
    int item;
    P(&sp->items);
    P(&sp->mutex);
    item = sp->buf[(++sp->front)%(sp->n)];
    V(&sp->mutex);
    V(&sp->slots);
    return item;
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