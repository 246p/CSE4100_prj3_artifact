#include "csapp.h"
#include <time.h>

#define MAX_CLIENT 100
#define ORDER_PER_CLIENT 10
#define STOCK_NUM 10
#define BUY_SELL_MAX 10

int main(int argc, char **argv) 
{
	pid_t pids[MAX_CLIENT];
	int runprocess = 0, status, i;

	int clientfd, num_client;
	char *host, *port, buf[MAXLINE], tmp[3];
	rio_t rio;

	if (argc != 5) {
		fprintf(stderr, "usage: %s <host> <port> <client#> <option> 0: show, 1: buy, 2: sell, 3=rand\n", argv[0]);
		exit(0);
	}

	host = argv[1];
	port = argv[2];
	num_client = atoi(argv[3]);
	clock_t start, end;
	start = clock();
/*	fork for each client process	*/
	while(runprocess < num_client){
		pids[runprocess] = fork();
		if(pids[runprocess] < 0)
			return -1;
		/*	child process		*/
		else if(pids[runprocess] == 0){
			printf("child %ld\n", (long)getpid());

			clientfd = Open_clientfd(host, port);
			Rio_readinitb(&rio, clientfd);
			srand((unsigned int) getpid());
			for(i=0;i<ORDER_PER_CLIENT;i++){
				int option = atoi(argv[4]); //= rand() % 3;
				if(option ==3) option=rand()%3;
				if(option == 0){//show
					strcpy(buf, "show\n");
				}
				else if(option == 1){//buy
					int list_num = rand() % STOCK_NUM + 1;
					int num_to_buy = rand() % BUY_SELL_MAX + 1;//1~10

					strcpy(buf, "buy ");
					sprintf(tmp, "%d", list_num);
					strcat(buf, tmp);
					strcat(buf, " ");
					sprintf(tmp, "%d", num_to_buy);
					strcat(buf, tmp);
					strcat(buf, "\n");
				}
				else if(option == 2){//sell
					int list_num = rand() % STOCK_NUM + 1; 
					int num_to_sell = rand() % BUY_SELL_MAX + 1;//1~10
					
					strcpy(buf, "sell ");
					sprintf(tmp, "%d", list_num);
					strcat(buf, tmp);
					strcat(buf, " ");
					sprintf(tmp, "%d", num_to_sell);
					strcat(buf, tmp);
					strcat(buf, "\n");
				}
				Rio_writen(clientfd, buf, strlen(buf));
				Rio_readnb(&rio, buf, MAXLINE);
				Fputs(buf, stdout);

				//usleep(1000000);
			}

			Close(clientfd);
			exit(0);
		}
		runprocess++;
	}
	for(i=0;i<num_client;i++){
		waitpid(pids[i], &status, 0);
	}
	end = clock();
	printf("Time: %lf\n", (double)(end - start)/CLOCKS_PER_SEC);
	return 0;
}
