//Barak Levy
//ID 203280185
//Ex3 server 
#include "threadpool.h"
#include <stdio.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <netdb.h>  
#include <stdlib.h> 
#include <string.h> 
#include <unistd.h> 
#include <stdbool.h>
#include <ctype.h>
#include <assert.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#define RFC1123FMT   "%a, %d %b %Y %H:%M:%S GMT"
//global header for the entire errors
#define HEADER      "HTTP/1.0 %s\r\nServer: webserver/1.0\r\nDate:%s\r\nContent-Type: text/html\r\nContent-Length: %s\r\nConnection: close\r\n\r\n<HTML><HEAD><TITLE>%s</TITLE></HEAD>\r\n<BODY><H4>%s</H4>\r\n%s\r\n</BODY></HTML>"    
#define MAX_LEN 4000
typedef struct//the input fields will store into the struct 
{   
    
    char * path;

}Request;
void usage();
int readAndResponse(void*);
char *get_mime_type(char *name);
void errorResponse(int errorNum,int fd,char* path);
void freeReq(Request* req);
bool sendFileResponse(int fd,char * path);
bool directoryContentsResponse(int fd,char * path);
void sendLoop(int fd,char * resp);
bool isNumber(char * number);
bool permissions(char * path);
bool checkValid(char * buff);
int main(int argc ,char** argv)
{      

    //check for valid parametres
    if(argc!=4||(!isNumber(argv[1])||!isNumber(argv[2])||!isNumber(argv[3]))||atoi(argv[1])<=0||atoi(argv[2])<=0||atoi(argv[3])<=0)
    {
        usage();
        exit(0);
    }
    int sockfd, newsockfd, portno;        
	struct sockaddr_in serv_addr;
    struct sockaddr_in addr;
    portno = atoi(argv[1]); //open the port 
    int max_num_of_req = atoi(argv[3]);
	serv_addr.sin_family = AF_INET;  
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
	serv_addr.sin_port = htons(portno); 
    sockfd = socket(PF_INET,SOCK_STREAM,0);
    socklen_t addr_len = sizeof(addr);    
    // create pool
    threadpool* pool =create_threadpool(atoi(argv[2]));//create pool if pool equal null mean has any usage problem in create pool func
    if(pool==NULL)
    {
        
        exit(0);
    }
	if (sockfd < 0)
    {
        perror("socket\n");
        destroy_threadpool(pool);
        exit(0);
    }  
	
    if (bind(sockfd,(struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("bind\n");
        destroy_threadpool(pool);
        exit(0);
    }               
    listen(sockfd,10);
    int* arr = (int *)malloc(sizeof(int )*max_num_of_req);
    if(arr==NULL)
    {
        perror("malloc\n");
        destroy_threadpool(pool);
        exit(0);
    }
    for(int i=0;i<max_num_of_req;i++)
    {
             
        arr[i] = accept(sockfd,(struct sockaddr*)&addr,&addr_len);
        if(arr[i]<0)
        {
            perror("accept\n");
            continue;
        }    
      dispatch(pool,readAndResponse,&arr[i]);//add job to the qeue 
     
    }
        
   destroy_threadpool(pool); 
   free(arr);
   close(sockfd);
}
void usage()
{
    printf("Usage:server<port><>pool-size><max-number-of-request>\n");
}
int readAndResponse(void * fd)//the function threads runs
{
    
    int f =*((int*) fd);
    char buff[MAX_LEN];
    memset(buff,'\0',MAX_LEN);
    char c [2];
    memset(c,'\0',2);
    while(read(f,c,1)!=0)//read only the first line
    {      
        if(strcmp(c,"\n")==0)
        {   
            strcat(buff,c);
            break;
        }
        strcat(buff,c);
    }
     if(!checkValid(buff))//check for 3 parameters 
     {
         errorResponse(400,f,"");
         return 1;
     }
    Request * req = (Request*)malloc (sizeof(Request));
    if(req==NULL)
    {
        errorResponse(500,f,"");    
        return 1;
    }
    req->path = NULL;
    char * method= strtok(buff," ");
    if(strcmp(method,"GET")!=0)//case not supported 
    {
        errorResponse(501,f,"");
        freeReq(req);
        return 1;
    }
    char * path = strtok(NULL," ");
    req->path = (char*)malloc(strlen(path)+1);
    if(req->path==NULL)
    {
        
        errorResponse(500,f,"");
        freeReq(req);
        return 1;
    }
    memset(req->path,'\0',strlen(path)+1);
    strcpy(req->path,path);
    
    char * protocol = strtok(NULL," ");
  
    if((strcmp(protocol,"HTTP/1.0\r\n")!=0)&&(strcmp(protocol,"HTTP/1.1\r\n")!=0))
    {       
        errorResponse(400,f,"");//bad request case
        freeReq(req);
        return 1;
    }
    
    if(req->path[0]!='/')
    {
        errorResponse(404,f,"");//not found case
        freeReq(req);
        return 1;

    }
    if(strcmp(req->path,"/")==0)//case only / fix to /./ main directory
    {
        free(req->path);
        req->path = (char*)malloc(4);
        if(req->path==NULL)
        {
             errorResponse(500,f,"");
             freeReq(req);//should free fields as well 
             return 1;
        }
        memset(req->path,'\0',4);
        strcpy(req->path,"/./");
    }
    else if((req->path[0])=='/'&&((req->path[1])=='/'))//case trying to get main cp directory 
    {
        errorResponse(403,f,"");
        freeReq(req);//should free fields as well 
        return 1;

    }
    else if(access(req->path+1,F_OK)==-1)//if the path dont exist 404 err
    {
        errorResponse(404,f,"");
        freeReq(req);//should free fields as well 
        return 1;
    }
    struct stat sb;
    stat(req->path+1, &sb);//get info about the path
    if((sb.st_mode & S_IFMT)==S_IFDIR)//case its a dir
    {
         
        if(req->path[strlen(req->path)-1]!='/')//case its a dir but dont end with / 302 err
        {
            errorResponse(302,f,req->path);
            freeReq(req);//should free fields as well 
            return 1;
        }
        else
        {
             if(!permissions(req->path+1))//case forrbidden request 
             {
                 errorResponse(403,f,"");
                 freeReq(req);
                 return 1;
             }
       
             char * temp = (char* )malloc(strlen(req->path)+12);
             if(temp==NULL)
             {
                  errorResponse(500,f,"");
                  freeReq(req);//should free fields as well 
                  return 1  ;

             }
             memset(temp,'\0',strlen(req->path)+12);
             strcpy(temp,req->path+1);
             strcat(temp,"index.html");
                                          
            if(access(temp,F_OK)!=-1)//if there is index.html inside 
            {   
                req->path = (char*)realloc(req->path,50);
                strcat(req->path,"index.html");
                stat(req->path+1, &sb);

                if(!(sb.st_mode & S_IROTH)||!permissions(req->path+1))//case forrbidden request  
                {
                     errorResponse(403,f,"");
                    
                } 
                else
                {
                     sendFileResponse(f,req->path+1);//send index.html 
                }
                freeReq(req);
                free(temp);
                return 1 ;  
                
            }
             else// return the contents of the directory in the format 
             {
               
                if(!directoryContentsResponse(f,req->path+1))
                {
                    errorResponse(500,f,"");//case server error (malloc..)
                    
                }
                freeReq(req);
                free(temp);
                return 1;         
             }
             
        }  
    }
    else if(!S_ISREG(sb.st_mode ))//case not a regular file
    {
        errorResponse(403,f,"");
        freeReq(req);
        return 1;
        
    }
    else if(!permissions(req->path+1))
    {
        //send 403 Forbidden response.
        errorResponse(403,f,"");
        freeReq(req);
        return 1;
    }
    else
    {
            //return the file
        if(!sendFileResponse(f,req->path+1))
        {
            errorResponse(500,f,req->path+1);
        
        }
        freeReq(req);        
    }
}
bool directoryContentsResponse(int fd,char * path)//build a table for the whole contents of the dir 
{
    char  lastModify [128];
    memset(lastModify,'\0',128);
    struct stat sb;
    stat(path, &sb);
    strftime(lastModify, sizeof(lastModify), RFC1123FMT, gmtime(&sb.st_mtime));
    struct dirent **namelist;
    int  n = scandir(path, &namelist, NULL, NULL);
    
    if (n == -1) 
    {       
        return false;
        
    } 
    char* htmlData  = (char*) malloc((n)*500);
    if(htmlData==NULL)
    {
        return false;
    }
    memset(htmlData,'\0',500*(n));
    sprintf(htmlData,"<HTML>\r\n<HEAD><TITLE>Index of %s</TITLE></HEAD>\r\n\r\n<BODY>\r\n<H4>Index of %s</H4>\r\n\r\n<table CELLSPACING=8>\r\n<tr><th>Name</th><th>Last Modified</th><th>Size</th></tr>\r\n\r\n",path,path);
    char temp [600];
    memset(temp,'\0',600);
    char temp2[200];
    memset(temp2,'\0',200);
    while (n--) //for each entity 
    {
  
        char * t = (char * )malloc(strlen(namelist[n]->d_name)+strlen(path)+1);
        if(t==NULL)
        {
           
            return false  ;

        }
        memset(t,'\0',strlen(namelist[n]->d_name)+strlen(path)+1);
        strcpy(t,path);
        strcat(t,namelist[n]->d_name);
        stat(t, &sb);
        strftime(lastModify, sizeof(lastModify), RFC1123FMT, gmtime(&sb.st_mtime));
       
       
        if((sb.st_mode & S_IFMT)==S_IFDIR)
        {
            sprintf(temp,"</tr><td><A HREF=\"%s/\">%s</A></td><td>%s</td>",namelist[n]->d_name,namelist[n]->d_name,lastModify);
            sprintf(temp2,"<td></td>\r\n");
         
        }
        else
        {
               sprintf(temp,"</tr><td><A HREF=\"%s\">%s</A></td><td>%s</td>",namelist[n]->d_name,namelist[n]->d_name,lastModify);
               sprintf(temp2,"<td>%ld</td></tr>",sb.st_size);
        }
        strcat(temp,temp2);
        strcat(htmlData,temp);
        free(namelist[n]);
        free(t);
    
    }
    strcat(htmlData,"</table>\r\n<HR>\r\n<ADDRESS>webserver/1.0</ADDRESS>\r\n</BODY></HTML>");
    
    int len = strlen(htmlData);
  
    char resp [400];
    memset(resp,'\0',400);
    time_t now;
    char timebuf[128];
    memset(timebuf,'\0',128);
    now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
    stat(path, &sb);
    strftime(lastModify, sizeof(lastModify), RFC1123FMT, gmtime(&sb.st_mtime));
    sprintf(resp,"HTTP/1.0 200 OK\r\nServer: webserver/1.0\r\nDate: %s\r\nContent-Type: text/html\r\nContent-Length: %d\r\nLast-Modified: %sConnection: close\r\n\r\n",timebuf,len,lastModify); 
    sendLoop(fd,resp);
    sendLoop(fd,htmlData);
    close(fd);
    free(htmlData);
    free(namelist);
    return true;
}
void sendLoop(int fd,char * resp)
{
    // int total = strlen(resp),bytes=0,sent=0;
    // do//write the request to the server 
    // {   
    //     bytes = write(fd,resp+sent,strlen(resp)-sent);
    
    //     if(bytes==0)
    //     {
    //         break;
    //     }
    //     sent+=bytes;
    // }
    // while(sent<total);
    int ret =write(fd,resp,strlen(resp));

}
bool sendFileResponse(int fd,char * path)//send any kind of file with 200 ok header 
{
    time_t now;
    char timebuf[128];
    memset(timebuf,'\0',128);
    now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));

    char  resp [600]; 
    memset(resp,'\0',600);
    FILE * file;
    file = fopen(path,"r");
    if(file==NULL)
    {
        perror("fopen\n");
        return false;
    }
    char  type[130];
    memset(type,'\0',130);
    strcpy(type,"Content-type: ");
    if(get_mime_type(path)==NULL)//get the type 
    {
        strcpy(type,"");
    }
    else
    {
        strcat(type,get_mime_type(path));
        strcat(type,"\r\n");
    }
    char  lastModify [128];
    memset(lastModify,'\0',128);
    struct stat sb;
    stat(path, &sb);
    strftime(lastModify, sizeof(lastModify), RFC1123FMT, gmtime(&sb.st_mtime));
    char * data =(char*) malloc(sb.st_size+1);
    if(data==NULL)
    {
        
        
        
        return false;
    }
    memset(data,'\0',sb.st_size+1);
    sprintf(resp,"HTTP/1.0 200 OK\r\nServer: webserver/1.0\r\nDate: %s\r\n%sContent-Length: %ld\r\nLast-Modified: %s\r\nConnection: close\r\n\r\n",timebuf,type,sb.st_size,lastModify);
    

   // for (int sent = 0; sent <sizeof(resp); sent += write(fd, resp+sent, strlen(resp)-sent)); 
    sendLoop(fd,resp);
    int r = 0;
    while(r = fread(data,1,sb.st_size+1,file)>0)
    {
        write(fd,data,sb.st_size+1);
       
    }
    free(data);
    fclose(file);
    close(fd);
    return true;
}
void errorResponse(int errorNum,int fd,char* path)//this function update the defined HEADER and send the required error to the socket 
{
    time_t now;
    char timebuf[128];
    memset(timebuf,'\0',128);
    now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
    char resp [450];
    memset(resp,'\0',450);
    if(errorNum==400)
    {
        
        sprintf(resp,HEADER,"400 Bad Request",timebuf,"113","400 Bad Request","400 Bad Request","Bad Request.");   
      
    } 
    if(errorNum==500)
    {
        
        sprintf(resp,HEADER,"500 Internal Server Error",timebuf,"144","500 Internal Server Error","500 Internal Server Error","Some server side error.");
       
        
    }
    if(errorNum==501)
    {
       
        sprintf(resp,HEADER,"501 Not supported",timebuf,"129","501 Not supported","501 Not supported","Method is not supported.");
      
    }
    if(errorNum==404)
    {
       
        sprintf(resp,HEADER,"404 Not Found",timebuf,"112","404 Not Found","404 Not Found","File not found.");
        

    }
    if(errorNum==403)
    {
        
        sprintf(resp,HEADER,"403 Forbidden",timebuf,"111","403 Forbidden","403 Forbidden","Access denied.");
    

    }
    if(errorNum==302)
    {       
        
        sprintf(resp, "HTTP/1.0 302 Found\r\nServer: webserver/1.0\r\nDate: %s\r\nLocation: %s/\r\nContent-Type: text/html\r\nContent-Length: 123\r\nConnection: close\r\n\r\nHTML><HEAD><TITLE>302 Found</TITLE></HEAD>\r\nBODY><H4>302 Found</H4>\r\nDirectories must end with a slash.\r\n</BODY></HTML>",timebuf,path);
   
        
    }
    sendLoop(fd,resp);
    close(fd);
 
}
char *get_mime_type(char *name) 
{
    char *ext = strrchr(name, '.');
    if (!ext) return NULL;
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".au") == 0) return "audio/basic";
    if (strcmp(ext, ".wav") == 0) return "audio/wav";
    if (strcmp(ext, ".avi") == 0) return "video/x-msvideo";
    if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0) return "video/mpeg";
    if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
    return NULL;
}
void freeReq(Request* req)
{
    if(req->path!=NULL)
    {
        free(req->path);
    }
   
    if(req!=NULL)
    {
        free(req);
    }
   

}
bool isNumber(char * number)
{
    for(int i =0;i<strlen(number);i++)
    {
        if(isdigit(number[i])==0)
        {
            return false;
        }
    }
    return true;
}
bool checkValid(char * buff)
{
    char * temp =(char*) malloc(strlen(buff)+1);
    memset(temp,'\0',strlen(buff)+1);
    strcpy(temp,buff);
    char *temp1 = strtok(temp," ");
    int count = 0;
    while(temp1!=NULL)
    {
        count ++;
        temp1 = strtok(NULL," ");
    }
    free(temp);
    if(count!=3)
    {
        return false;
    }
    return true;

    
}
bool permissions(char * path)//parsing the path into dirs and files and check for each entity the permission 
{
    struct stat statbuf;
   	stat(path, &statbuf); 
    

   	char *token;
	char temp[MAX_LEN]; 
	char temp2[MAX_LEN]; 
	strcpy(temp2,"");
	strcpy(temp,path);
	token = strtok(temp, "/"); 
	while( token != NULL) 
    {
		strcat(temp2,token); 
		stat(temp2, &statbuf); 
		if(S_ISDIR(statbuf.st_mode) && !(statbuf.st_mode &S_IXOTH)) 
			return false;
		else if(!(statbuf.st_mode & S_IROTH)) 
			return false;
    	token = strtok(NULL, "/"); 
    	strcat(temp2,"/");
    }
    return true;
}
