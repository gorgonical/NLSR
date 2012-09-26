#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include<ctype.h>
#include<stdarg.h>
#include <sys/types.h>
#include <pwd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <assert.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


#include <ccn/ccn.h>
#include <ccn/uri.h>
#include <ccn/keystore.h>
#include <ccn/signing.h>
#include <ccn/schedule.h>
#include <ccn/hashtb.h>


#include "utility.h"


char * getLocalTimeStamp(void)
{
	char *timestamp = (char *)malloc(sizeof(char) * 16);
	time_t ltime;
	ltime=time(NULL);
	struct tm *tm;
	tm=localtime(&ltime);
  
	sprintf(timestamp, "%04d%02d%02d%02d%02d%02d", tm->tm_year+1900, tm->tm_mon+1, 
		tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);

	return timestamp;
}

char * getGmTimeStamp(void)
{
	char *timestamp = (char *)malloc(sizeof(char) * 16);
	time_t gtime;
	gtime=time(NULL);
	struct tm *tm;
	tm=gmtime(&gtime);
  
	sprintf(timestamp, "%04d%02d%02d%02d%02d%02d", tm->tm_year+1900, tm->tm_mon+1, 
		tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);

	return timestamp;
}




long int 
get_current_time_sec(void)
{
	struct timeval now;
	gettimeofday(&now,NULL);
	return now.tv_sec;
}


void
get_current_timestamp_micro(char * microSec)
{
	struct timeval now; 
	gettimeofday(&now, NULL);
	sprintf(microSec,"%ld%06ld",now.tv_sec,(long int)now.tv_usec);
}


long int
get_time_diff(const char *time1, const char *time2)
{
	long int diff_secs;

	long int time1_in_sec,	time2_in_sec;

	char *time1_sec=(char *)malloc(strlen(time1)-6+1);
	memset(time1_sec,0,strlen(time1)-6+1);
	memcpy(time1_sec,time1,strlen(time1)-6);

	char *time2_sec=(char *)malloc(strlen(time2)-6+1);
	memset(time2_sec,0,strlen(time2)-6+1);
	memcpy(time2_sec,time2,strlen(time2)-6);

	time1_in_sec=strtol(time1_sec,NULL,10);
	time2_in_sec=strtol(time2_sec,NULL,10);

	diff_secs=time1_in_sec-time2_in_sec;

	free(time1_sec);
	free(time2_sec);

	return diff_secs;
}


void  
startLogging(char *loggingDir)
{
	struct passwd pd;
	struct passwd* pwdptr=&pd;
	struct passwd* tempPwdPtr;
	char *pwdbuffer;
	int  pwdlinelen = 200;
	char *logDir;
	char *logFileName;
	char *ret;
	char *logExt;
	char *defaultLogDir;	
	int status;
	struct stat st;
	int isLogDirExists=0;
	char *time=getLocalTimeStamp();

	pwdbuffer=(char *)malloc(sizeof(char)*200);
	memset(pwdbuffer,0,200);		
	logDir=(char *)malloc(sizeof(char)*200);
	memset(logDir,0,200);
	logFileName=(char *)malloc(sizeof(char)*200);
	memset(logFileName,0,200);
	logExt=(char *)malloc(sizeof(char)*5);
	memset(logExt,0,5);
	defaultLogDir=(char *)malloc(sizeof(char)*10);
	memset(defaultLogDir,0,10);

	memcpy(logExt,".log",4);
	logExt[4]='\0';
	memcpy(defaultLogDir,"/nlsrLog",9);
	defaultLogDir[9]='\0';

	if(loggingDir!=NULL)
 	{
		if( stat( loggingDir, &st)==0)
		{
			if ( st.st_mode & S_IFDIR )
			{
				if( st.st_mode & S_IWUSR)
				{
					isLogDirExists=1;
					memcpy(logDir,loggingDir,strlen(loggingDir)+1);
				}
				else printf("User do not have write permission to %s \n",loggingDir);
			}
			else printf("Provided path for %s is not a directory!!\n",loggingDir);
    		}
  		else printf("Log directory: %s does not exists\n",loggingDir);
	} 
  
	if(isLogDirExists == 0)
  	{
		if ((getpwuid_r(getuid(),pwdptr,pwdbuffer,pwdlinelen,&tempPwdPtr))!=0)
     			perror("getpwuid_r() error.");
  		else
  		{
			memcpy(logDir,pd.pw_dir,strlen(pd.pw_dir)+1);	
			memcpy(logDir+strlen(logDir),defaultLogDir,strlen(defaultLogDir)+1);	
			if(stat(logDir,&st) != 0)
				status = mkdir(logDir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);	
		}
	}	
 	memcpy(logFileName,logDir,strlen(logDir)+1);	
	if( logDir[strlen(logDir)-1]!='/')
	{
		memcpy(logFileName+strlen(logFileName),"/",1);
		memcpy(logFileName+strlen(logFileName),"\0",1);	
	}	
	memcpy(logFileName+strlen(logFileName),time,strlen(time)+1);	
	memcpy(logFileName+strlen(logFileName),logExt,strlen(logExt)+1);	
	ret=(char *)malloc(strlen(logFileName)+1);
	memset(ret,0,strlen(logFileName)+1);
       	memcpy(ret,logFileName,strlen(logFileName)+1); 

	setenv("NLSR_LOG_FILE",ret,1);

	free(time);	
	free(logDir);
	free(logFileName);	
	free(pwdbuffer);	
	free(logExt);
	free(defaultLogDir);
	free(ret);	
}


void 
writeLogg(const char *source_file, const char *function, const int line, const char *format, ...)
{
	char *file=getenv("NLSR_LOG_FILE");	
	if (file != NULL)
	{
		FILE *fp = fopen(file, "a");

		if (fp != NULL)
		{            
			struct timeval t;
			gettimeofday(&t, NULL);
			fprintf(fp,"%ld.%06u - %s, %s, %d :",(long)t.tv_sec , (unsigned)t.tv_usec , source_file , function , line);        
			va_list args;
			va_start(args, format);
			vfprintf(fp, format, args);
			fclose(fp);
			va_end(args);	
		}
    	}
}

