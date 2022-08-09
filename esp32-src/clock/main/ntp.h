#define CONFIG_SNTP_TIME_SERVER "pool.ntp.org"

void initialize_sntp(void);
void obtain_time(void);
void ntp_app_main(void);

