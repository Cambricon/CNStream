#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#ifdef _WIN32

#else
#include <sys/time.h>
#endif
#include "VCATime.h"

CTickTime::CTickTime(void)
{
	ProcessStart();
}

void CTickTime::ProcessStart(void)
{
	start_time= GetTickTime();
}

double CTickTime::GetTickTime(void)
{
#ifdef _WIN32
	return ((double)clock()/CLOCKS_PER_SEC*1000.0);
#else
	struct timeval t_start;
	gettimeofday(&t_start, NULL);
	return ((double)t_start.tv_sec)*1000.0+(double)t_start.tv_usec/1000.0;
#endif
}

double CTickTime::ProcessTickTime(void)
{
	current_time=GetTickTime();
	going_time=current_time-start_time;
	start_time=current_time;
	return going_time;
}

void CTickTime::PrintProcessTickTime(const char *p_process_name)
{
	printf("\r\n%s(%.3f ms)", p_process_name, ProcessTickTime());
}

void CTickTime::Start(void)
{
	start_time= GetTickTime();
}

double CTickTime::GoingTime(void)
{
	current_time= GetTickTime();
	going_time=current_time-start_time;
	return going_time;
}

unsigned long CTickTime::GoingTimeUL(void)
{
	return (unsigned long)GoingTime();
}

long CTickTime::GoingTimeL(void)
{
	return (long)GoingTime();
}


bool GetTime(VCATime_t *p_time)
{
	if(p_time==NULL)
	{
		return false;
	}
	time_t temp_time;
	time(&temp_time);
	struct tm *tt=localtime(&temp_time);
	if(tt==NULL)
	{
		return false;
	}

	p_time->year=tt->tm_year+1900;
	p_time->mon=tt->tm_mon+1;
	p_time->mday=tt->tm_mday;
	p_time->hour=tt->tm_hour;
	p_time->min=tt->tm_min;
	p_time->sec=tt->tm_sec;
	return true;
}

bool GetTime(unsigned long long time_sec, VCATime_t *p_time)
{
	if(p_time==NULL)
	{
		return false;
	}
	time_t temp_time=time_sec;
	struct tm *tt=localtime(&temp_time);
	if(tt==NULL)
	{
		return false;
	}

	p_time->year=tt->tm_year+1900;
	p_time->mon=tt->tm_mon+1;
	p_time->mday=tt->tm_mday;
	p_time->hour=tt->tm_hour;
	p_time->min=tt->tm_min;
	p_time->sec=tt->tm_sec;
	return true;
}

bool GetTimeString(char *p_buffer, int time_format)
{
	return GetTimeString(GetTime(), p_buffer, time_format);
}


bool GetTimeString(unsigned long long time_sec, char *p_buffer, int time_format)
{
	if(p_buffer==NULL)
	{
		return false;
	}
	VCATime_t current_time;
	GetTime(time_sec, &current_time);
	if(time_format==0)
	{
		sprintf(p_buffer, "%04u-%02u-%02u %02u:%02u:%02u", current_time.year,current_time.mon,current_time.mday,current_time.hour,current_time.min,current_time.sec);
	}
	else if(time_format==1)
	{
		sprintf(p_buffer, "%04u%02u%02u%02u%02u%02u", current_time.year,current_time.mon,current_time.mday,current_time.hour,current_time.min,current_time.sec);
	}
	else
	{
		return false;
	}
	return true;
}


unsigned long long GetTime(void)
{
	time_t tm;
	time(&tm);
	return (unsigned long long)tm;
}



