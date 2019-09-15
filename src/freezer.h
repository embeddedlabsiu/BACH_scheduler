#ifndef FREEZER_H__
#define FREEZER_H__

//#define FREEZER_TEST
enum {
	FRZR_THAWED = 0,
	FRZR_FROZEN,
	FRZR_FREEZING,
	FRZR_UNINITIALIZED,
	FRZR_STATES_END__
};

void frzr_init(pid_t pid, const char *id);
void frzr_shut(const char *id);

void frzr_setstate(const char *id, int state);
int frzr_getstate(const char *id);

#endif /* FREEZER_H__ */
