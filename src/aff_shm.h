#ifndef AFF_SHM__
#define AFF_SHM__

/* Common things for executor and app contexts (mostly about shared memory interface)*/

#include <inttypes.h>
#include "tsc.h"

#define AFF_SHM_SIZE 4096
#define AFF_MAX_PATH 32

/* XXX: We might need to define some memory barrier functions */

static inline void aff_shm_set_fd_app(void *shm, uint32_t x);
static inline void aff_shm_set_fd_executor(void *shm, uint32_t x);
static inline void aff_shm_set_cpus_arch(void *shm, uint32_t x);
static inline void aff_shm_set_cpus_assigned(void *shm, uint32_t x);
static inline void aff_shm_set_cpus_requested(void *shm, uint32_t x);
static inline void aff_shm_set_app_pid(void *shm, uint32_t x);
static inline void aff_shm_set_app_handler(void *shm, unsigned long x);

static inline uint32_t aff_shm_get_fd_app(void *shm);
static inline uint32_t aff_shm_get_fd_executor(void *shm);
static inline uint32_t aff_shm_get_cpus_arch(void *shm);
static inline uint32_t aff_shm_get_cpus_assigned(void *shm);
static inline uint32_t aff_shm_get_cpus_requested(void *shm);
static inline uint32_t aff_shm_get_app_pid(void *shm);

/* shared memory map: offsets for each variable */
#define FD_APP_         0
#define FD_EXECUTOR_    (FD_APP_        + sizeof(uint32_t))
#define CPUS_ARCH_      (FD_EXECUTOR_   + sizeof(uint32_t))
#define CPUS_ASSIGNED_  (CPUS_ARCH_     + sizeof(uint32_t))
#define APP_HANDLER_    (CPUS_ASSIGNED_ + sizeof(uint32_t))
#define APP_PID_				(APP_HANDLER_		+ sizeof(unsigned long))
#define EXECUTOR_END_   (APP_PID_			  + sizeof(uint32_t))
/* These are written by the app: keep it on a different cache line */
#define CPUS_REQUESTED_ (CACHE_LINE_SIZE) /* usually 64 bytes */

static inline void
aff_shm_set_uint32(void *shm, uint32_t x, int offset)
{
	volatile uint32_t *p = shm + offset;
	*p = x;
}

static inline uint32_t
aff_shm_get_uint32(void *shm, int offset)
{
	volatile uint32_t *p = shm + offset;
	return *p;
}

static inline void
aff_shm_set_ul(void *shm, unsigned long x, int offset)
{
	volatile unsigned long *p = shm + offset;
	*p = x;
}

static inline unsigned long
aff_shm_get_ul(void *shm, int offset)
{
	volatile unsigned long *p = shm + offset;
	return *p;
}

/* warning, preprocessor abuse below */
#define DECL_AFF_SHM_ACCESSOR_U32(_fn_suffix, _off) \
static inline void                                  \
aff_shm_set_ ## _fn_suffix(void *shm, uint32_t x)   \
{ aff_shm_set_uint32(shm, x, _off); }               \
                                                    \
static inline uint32_t                              \
aff_shm_get_ ## _fn_suffix(void *shm)               \
{ return aff_shm_get_uint32(shm, _off); }

#define DECL_AFF_SHM_ACCESSOR_UL(_fn_suffix, _off)     \
static inline void                                     \
aff_shm_set_ ## _fn_suffix(void *shm, unsigned long x) \
{ aff_shm_set_ul(shm, x, _off); }                      \
                                                       \
static inline unsigned long                            \
aff_shm_get_ ## _fn_suffix(void *shm)                  \
{ return aff_shm_get_ul(shm, _off); }

DECL_AFF_SHM_ACCESSOR_U32(fd_app,         FD_APP_)
DECL_AFF_SHM_ACCESSOR_U32(fd_executor,    FD_EXECUTOR_)
DECL_AFF_SHM_ACCESSOR_U32(cpus_arch,      CPUS_ARCH_)
DECL_AFF_SHM_ACCESSOR_U32(cpus_assigned,  CPUS_ASSIGNED_)
DECL_AFF_SHM_ACCESSOR_UL(app_handler,     APP_HANDLER_)
DECL_AFF_SHM_ACCESSOR_U32(cpus_requested, CPUS_REQUESTED_)
DECL_AFF_SHM_ACCESSOR_U32(app_pid,				APP_PID_)

#undef DECL_AFF_SHM_ACCESSOR_U32
#undef DECL_AFF_SHM_ACCESSOR_UL

/* don't need those no more */
#undef FD_APP_
#undef FD_EXECUTOR_
#undef CPUS_ARCH_
#undef CPUS_ASSIGNED_
#undef APP_HANDLER_
#undef EXEUCTOR_END_
#undef CPUS_REQUESTED_

#endif /* AFF_SHM__ */
