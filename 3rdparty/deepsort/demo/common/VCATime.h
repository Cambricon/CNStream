#pragma once


class CTickTime
{
public:
	CTickTime(void);
	void ProcessStart(void);
	double GetTickTime(void);
	double ProcessTickTime(void);
	void PrintProcessTickTime(const char *p_process_name);

	void Start(void);
	double GoingTime(void);
	unsigned long GoingTimeUL(void);
	long GoingTimeL(void);

	double start_time;
	double current_time;
	double going_time;
};

typedef struct
{
	int year;
	int mon;
	int mday;
	int hour;
	int min;
	int sec;
}VCATime_t;

bool GetTime(VCATime_t *p_time);
bool GetTime(unsigned long long time_sec, VCATime_t *p_time);
unsigned long long GetTime(void);
bool GetTimeString(char *p_buffer, int time_format=0); //0=2017-03-01 10:53:00, 1=20170301105300
bool GetTimeString(unsigned long long time_sec, char *p_buffer, int time_format=0); //0=2017-03-01 10:53:00, 1=20170301105300

