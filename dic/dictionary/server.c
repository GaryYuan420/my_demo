#include "server.h"

int main(int argc, char* argv[])
{
	int socketfd;
	struct sockaddr_in severaddr;
	sqlite3 *db;
	int accpetfd;
	pid_t pid;
	char * errmsg;

	if(argc != 3){
		printf("Usage:%s serverip  port.\n", argv[0]);
		return -1;
	}

	signal(SIGCHLD, sig_child_handle);	

	if(sqlite3_open(DATABASE, &db) != SQLITE_OK){
		printf("%s\n", sqlite3_errmsg(db));
		return -1;
	}else{
		printf("open DATABASE success.\n");
	}
	if(sqlite3_exec(db, "create table if not exists "USRTABLE"(name text primary key, password text, time text);",
				NULL, NULL, &errmsg) != SQLITE_OK)
	{
		printf("%s\n", errmsg);
	}
	else
	{
		printf("Create or open "USRTABLE" success.\n");
	}
	
	if(sqlite3_exec(db, "create table if not exists "HISTABLE"(name text, word text, time text);",
				NULL, NULL, &errmsg) != SQLITE_OK)
	{
		printf("%s\n", errmsg);
	}
	else
	{
		printf("Create or open "HISTABLE" success.\n");
	}

	if ((socketfd = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
		perror ("socket");
		exit (1);
	}

	int b_reuse = 1;
	setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &b_reuse, sizeof(int));
	
	bzero(&severaddr, sizeof(severaddr));
	severaddr.sin_family = AF_INET;
	severaddr.sin_port = htons(atoi(argv[2]));
	//PR("server ip is %s, PORT is %s\n",argv[1],argv[2]);
	if(inet_pton(AF_INET, argv[1],(void *)&severaddr.sin_addr) != 1){
		perror("inet_pton");
		exit(1);	
	}
	if(bind(socketfd, (struct sockaddr *)&severaddr, sizeof(severaddr)) < 0){
		perror("bind");
		exit(1);		
	}
	if(listen(socketfd, 5) < 0){
		perror("listen");
		exit(1);
	}
	while(1){
		if((accpetfd = accept(socketfd, NULL,NULL)) < 0){
			perror("accept");
			exit(1);	
		}
		PR("accept!\n");
		if((pid = fork()) < 0){
			perror("fork");
			break;
		}
		else if(pid == 0){
			close(socketfd);
			handler(&accpetfd,db);
			break;
		}
		else{
			close(accpetfd);
		}
	}
	return 0;
	
}

void sig_child_handle(int signo)
{
	if(SIGCHLD == signo) {
		waitpid(-1, NULL,  WNOHANG);
	}
}

void handler(void *arg, sqlite3 *db)
{
	int socketfd = *(int *)arg;
	struct msg dicmsg;
	int ret;
	while(1){
		ret = read(socketfd, &dicmsg, sizeof(struct msg));
		if(ret < 0){
			printf("error!\n");
			break;
		}
		if(ret == 0){
			printf("client exited!\n");
			break;
		}
		switch (dicmsg.type)
			{
			case R:
				do_register(socketfd, &dicmsg, db);
				break;
			case L:
				do_login(socketfd, &dicmsg, db);
				break;
			case Q:
				do_query(socketfd, &dicmsg, db);
				break;
			case H:
				do_history(socketfd, &dicmsg, db);
				break;
			default:
				break;
			}
	}
	return ;
}

int do_register(int socketfd, struct msg* dicmsg, sqlite3 * db)
{
	char* errmsg;
	char sql[128];
	char time_str[32];
	get_time(time_str);
	PR("time is %s,strlen is %d,acturaly size is %d\n",time_str,strlen(time_str),strlen("2018-4-27 22:42:56"));
	sprintf(sql, "insert into "USRTABLE" values('%s', '%s', '%s');", dicmsg->name, dicmsg->data, time_str);
	PR("sql:%s\n",sql);
	if(sqlite3_exec(db,sql, NULL, NULL, &errmsg) !=SQLITE_OK )	{
		printf("%s\n", errmsg);
		strcpy(dicmsg->data, "usr name already exist.");
	} else {
		printf("client  register ok!\n");
		strcpy(dicmsg->data, "OK!");
	}

	if(send(socketfd, dicmsg, sizeof(struct msg), 0) < 0)
	{
		perror("fail to send");
		return 0;
	}

	return 0;
}

int do_login(int socketfd, struct msg* dicmsg, sqlite3 * db)
{
	char* errmsg;
	char sql[128];
	char time_str[32];
	int nrow;
	int ncloumn;
	char **resultp;
	get_time(time_str);
	PR("time is %s\n",time_str);
	sprintf(sql, "select * from "USRTABLE" where name = '%s' and password = '%s';", dicmsg->name, dicmsg->data);
	PR("sql:%s\n",sql);
	if(sqlite3_get_table(db,sql, &resultp, &nrow, &ncloumn, &errmsg) !=SQLITE_OK )	{
		printf("%s\n", errmsg);
		return 0;
	} else {
		printf("client  register ok!\n");
		strcpy(dicmsg->data, "OK!");
	}
	
	if(nrow == 1)
	{
		strcpy(dicmsg->data, "OK");
		send(socketfd, dicmsg, sizeof(struct msg), 0);
		return 1;
	}

	if(nrow == 0) // 密码或者用户名错误
	{
		strcpy(dicmsg->data,"usr/passwd wrong.");
		send(socketfd, dicmsg, sizeof(struct msg), 0);
	}
	return 0;
}

int do_query(int socketfd, struct msg* dicmsg, sqlite3 * db)
{
	char* errmsg;
	char word[64];
	char sql[128];
	char time_str[32];

	strcpy(word, dicmsg->data);
	if(do_search(dicmsg, word) == 1){
		get_time(time_str);
		sprintf(sql, "insert into "HISTABLE" values('%s', '%s', '%s');", dicmsg->name, word, time_str);
		PR("sql:%s\n",sql);
		if(sqlite3_exec(db,sql, NULL, NULL, &errmsg) !=SQLITE_OK )	{
			printf("%s\n", errmsg);
		} 
	}
	send(socketfd, dicmsg, sizeof(struct msg), 0);
	return 0;
	
}

int do_search(struct msg * dicmsg, char* word)
{
	FILE *fp;
	int len = 0;
	char tmp[512] = {};
	int result;
	char *p;
	if((fp = fopen(DICNAME, "r")) == NULL){
		perror("fopen");
		strcpy(dicmsg->data, "failed to open dictionary file\n" );
		return -1;
	}
	len = strlen(word);
	while(fgets(tmp, 512, fp) != NULL){
		result = strncmp(tmp, word, len);
		if(result < 0)
			continue;
		else if(result > 0 || ((result == 0) && (tmp[len] != ' '))){
			strcpy(dicmsg->data, "Not found!\n");
			break;
		}
		p = tmp + len;
		while(*p++ == ' ');
		strcpy(dicmsg->data, p - 1);
		fclose(fp);
		return 1;
			
	}
	strcpy(dicmsg->data, "Not found!\n");
	fclose(fp);
	return 0;
	
}

int do_history(int socketfd, struct msg* dicmsg, sqlite3 * db)
{
	char sql[128];
	char *errmsg;

	sprintf(sql,  "select * from "HISTABLE" where name = '%s';", dicmsg->name);
	
	if(sqlite3_exec(db, sql, his_callback, (void *)&socketfd, &errmsg)!= SQLITE_OK)	{
		printf("%s\n", errmsg);
	} else {
		printf("Query record done.\n");
	}
	dicmsg->data[0] = '\0';
	send(socketfd, dicmsg, sizeof(struct msg), 0);
	return 0;
}

int his_callback(void* arg,int f_num,char** f_value,char** f_name)
{
	int socketfd = *(int *)arg;
	struct msg dicmsg;
	sprintf(dicmsg.data, "%s , %s", f_value[1], f_value[2]);

	send(socketfd, &dicmsg, sizeof(struct msg), 0);
	return 0;
	
}

int get_time(char *data)
{
	time_t t;
	struct tm *tp;

	time(&t);

	tp = localtime(&t);
	sprintf(data, "%d-%d-%d %d:%d:%d", tp->tm_year + 1900, tp->tm_mon+1, tp->tm_mday, 
			tp->tm_hour, tp->tm_min , tp->tm_sec);

	return 0;
}



