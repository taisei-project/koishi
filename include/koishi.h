
#ifndef KOISHI_H
#define KOISHI_H

/**
 * @brief Annotates all publicly visible API functions
 */
#if defined _WIN32 || defined __CYGWIN__
	#if defined BUILDING_KOISHI
		#define KOISHI_API __declspec(dllexport)
	#elif defined KOISHI_DLLIMPORT
		#define KOISHI_API __declspec(dllimport)
	#else
		#define KOISHI_API
	#endif
#else
	#if defined BUILDING_KOISHI && defined __GNUC__
		#define KOISHI_API __attribute__((visibility("default")))
	#else
		#define KOISHI_API
	#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/**
 * @defgroup coroutine API for working with coroutines.
 * @{
 */

/**
 * @brief State of a #koishi_coroutine_t instance.
 */
enum koishi_state {
	KOISHI_SUSPENDED,  /**< The coroutine is suspended and may be resumed with #koishi_resume. */
	KOISHI_RUNNING,    /**< The coroutine is currently executing and may be yielded from with #koishi_yield. Only up to one coroutine may be running per thread at all times. */
	KOISHI_DEAD,       /**< The coroutine has finished executing and may be recycled with #koishi_recycle or destroyed with #koishi_deinit. */
};

/**
 * @brief A coroutine instance.
 *
 * This struct must be initialized with #koishi_init before use.
 */
#ifdef BUILDING_KOISHI
typedef struct koishi_coroutine koishi_coroutine_t;
struct koishi_coroutine_pub {
#else
typedef struct koishi_coroutine {
#endif
	/**
	 * @brief Recycle-safe data region.
	 *
	 * When the coroutine is in the #KOISHI_DEAD state, these fields may be safely
	 * aliased and used for any purpose by user code. The coroutine and its stack
	 * may then be recycled with #koishi_recycle. This may be used to implement
	 * pooling. Note that the act of recycling will overwrite the user data.
	 */
	void *recycle_safe[2];

	/**
	 * @brief Private data reserved for the implementation. Don't mess with it.
	 * @private
	 */
	void *_private[6];
#ifdef BUILDING_KOISHI
};
#else
} koishi_coroutine_t;
#endif

/**
 * @brief A coroutine entry point.
 *
 * The entry point is a function that is called inside a coroutine context when
 * it is resumed for the first time.
 *
 * Once the entry point returns, control flow jumps back to the last #koishi_resume
 * call for this coroutine, as if it yielded. Its state is set to #KOISHI_DEAD
 * and it may not be resumed again.
 *
 * @param data User data that was passed to the first call to #koishi_resume.
 *
 * @return Value to be returned from the corresponding #koishi_resume call.
 */
typedef void *(*koishi_entrypoint_t)(void *data);

/**
 * @brief Initialize a #koishi_coroutine_t structure.
 *
 * This function must be called before using any of the other APIs with a particular
 * coroutine instance. It allocates a stack at least @p min_stack_size bytes big
 * and sets up an initial jump context.
 *
 * After this function returns, the coroutine is in the #KOISHI_SUSPENDED state.
 * When resumed (see #koishi_resume), @p entry_point will begin executing in the
 * coroutine's context.
 *
 * @param co The coroutine to initialize.
 * @param min_stack_size Minimum size of the stack. The actual size will be a multiple of the system page size and at least two pages big. If 0, the default size will be used (currently 65536).
 * @param entry_point Function that will be called when the coroutine is first resumed.
 */
KOISHI_API void koishi_init(koishi_coroutine_t *co, size_t min_stack_size, koishi_entrypoint_t entry_point);

/**
 * @brief Recycle a previously initialized coroutine.
 *
 * This is a light-weight version of #koishi_init. It will set up a new context,
 * but reuse the existing stack, if allowed by the implementation. This is useful
 * for applications that want to create lots of short-lived coroutines fairly often.
 * They can avoid expensive stack allocations and deallocations by pooling and
 * recycling completed tasks.
 *
 * @param co The coroutine to recycle. It must be initialized.
 * @param entry_point Function that will be called when the coroutine is first resumed.
 */
KOISHI_API void koishi_recycle(koishi_coroutine_t *co, koishi_entrypoint_t entry_point);

/**
 * @brief Deinitialize a #koishi_coroutine_t structure.
 *
 * This will free the stack and any other resources associated with the coroutine.
 *
 * Memory allocated for the structure itself will not be freed, this is your
 * responsibility.
 *
 * After calling this function, the coroutine becomes invalid, and must not be
 * passed to any of the API functions other than #koishi_init. In particular,
 * it **may not** be recycled.
 *
 * @param co The coroutine to deinitialize.
 */
KOISHI_API void koishi_deinit(koishi_coroutine_t *co);

/**
 * @brief Resume a suspended coroutine.
 *
 * Transfers control flow to the coroutine context, putting it into the
 * #KOISHI_RUNNING state. The calling context is put into the #KOISHI_SUSPENDED
 * state.
 *
 * If the coroutine is resumed for the first time, \p arg will be passed
 * as a parameter to its entry point (see #koishi_entrypoint_t). Otherwise, it
 * will be returned from the corresponding #koishi_yield call.
 *
 * This function returns when the coroutine yields or finishes executing.
 *
 * @param co The coroutine to jump into. Must be in the #KOISHI_SUSPENDED state.
 * @param arg A value to pass into the coroutine.
 *
 * @return Value returned from the coroutine once it yields or returns.
 */
KOISHI_API void *koishi_resume(koishi_coroutine_t *co, void *arg);

/**
 * @brief Suspend the currently running coroutine.
 *
 * Transfers control flow out of the coroutine back to where it was last resumed,
 * putting it into the #KOISHI_SUSPENDED state. The calling context is put into
 * the #KOISHI_RUNNING state.
 *
 * This function must be called from a real coroutine context.
 *
 * This function returns when and if the coroutine is resumed again.
 *
 * @param arg Value to return from the corresponding #koishi_resume call.
 * @return Value passed to a future #koishi_resume call.
 */
KOISHI_API void *koishi_yield(void *arg);

/**
 * @brief Query the state of a coroutine.
 *
 * @return One of the tree possible states.
 * 		- #KOISHI_SUSPENDED
 * 		- #KOISHI_RUNNING
 * 		- #KOISHI_DEAD
 */
KOISHI_API int koishi_state(koishi_coroutine_t *co);

/**
 * @brief Query the currently running coroutine context.
 *
 * @return The coroutine currently running on this thread. This function may be
 * called from the thread's main context as well, in which case it returns a
 * pseudo-coroutine that represents that context. Attempting to resume or yield
 * from such pseudo-coroutines leads to undefined behavior.
 */
KOISHI_API koishi_coroutine_t *koishi_active(void);

/**
 * @}
 * @defgroup util Utility functions.
 * @{
 */

/**
 * @brief Query the system's page size.
 *
 * The page size is queried only once and then cached.
 *
 * @return The page size in bytes.
 */
KOISHI_API size_t koishi_util_page_size(void);

/**
 * @brief Query the real stack size for a given minimum.
 *
 * This function computes the exact stack size that #koishi_init would allocate
 * given \p min_size.
 *
 * @return The real stack size. It is the closest multiple of the system page
 * size that is equal or greater than double the pace size.
 */
KOISHI_API size_t koishi_util_real_stack_size(size_t min_size);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
